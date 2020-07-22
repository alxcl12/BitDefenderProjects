// MessageServer.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"

// communication library
#include "communication_api.h"
#include <windows.h>
#include "ThreadPool.h"

/*
Communication protocol will be this:
        1. Client/Server sends a fixed size structure, containg a Cmd field (see description on CM_HEADER) and a Size field, which will contain the size in bytes of DataBuffer that will be sent/received afterwards
        2. Client/Server sends DataBuffer with Size same as the one in the Header

    Receiver Client/Server will receive the data in the same manner, firstly the header, then the actual DataBuffer


    At start, client will attempt connection with server and wait for response, depending on which he remains or not
    If something fails server side, client receives header with some Cmd field, does not matter what, and buffer contains error message which will be printed
    All sends will be protected by a mutex object in order to have the ability to send a file asynchronous. This will have the effect that headers and buffers do not mix
    If the protocol involved only one send, this mutex would not be needed
*/



/*
Header structure, will be sent on every communication with server
Valid Cmd values:
1...10 - commands according to documentation
55 - about to send a file, buffer contains it's name
88 - part of file, buffer contains max 4kb of that file
77 - file was successfully sent, buffer contains file name

Size field contains size in bytes of buffer that is coming after header
*/
typedef struct _CM_HEADER
{
    DWORD Cmd; 
    DWORD Size;
} CM_HEADER;

/*
Client structure, will be used to modelate a connected client

Client field contains connected client

ConnectedStatus field reveals what is the current state of the connection:
0 - no user is connected
1 - user is connected
2 - empty, will be used to signal in client array that position is empty

User field will contain name of the user, or NULL if no user is connected

Position field will contain index of client inside client array

IMPORTANT: ConnectedStatus and User fields can not be accessed by multiple threads at a time so access to this fields will be managed by gMutexClients for the entire array of clients
*/
typedef struct _CLIENT
{
    CM_SERVER_CLIENT* Client;
    DWORD ConnectedStatus; 
    char* User; 
    int Position; 
} CLIENT;

PSRWLOCK gRWLockRegFile;    //SRWLock for access to registration file
CLIENT** gClients;          //Array of pointers to CLIENT structure, for keeping track of connected clients and their status
int gNumberThreads;         //Number of maximum concurrent clients connected to server
HANDLE gMutexClients;       //Mutex to sync access to clients array
HANDLE gMutexSend;          //Mutex to sync sending of data in order to have the ability to send file asynchronous

CM_SERVER* gServer;         //Server must be declared global in order to do cleanup since CTRL-C is the only way to stop server, according to documentation
CM_THREAD_POOL* gPool;      //Same goes for thread pool

/*
Determine max of 2 DWORDs
*/
CM_SIZE Max(DWORD Head, DWORD Size)
{
    if (Head > Size)
    {
        return (CM_SIZE)Head;
    }
    else
    {
        return (CM_SIZE)Size;
    }
}

/*
Receives header from client into Header given parameter
Returns 0 on success, -1 on error
*/
int ReceiveHeader(CM_SERVER_CLIENT* Client, CM_HEADER** Header)
{
    if (Client == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    CM_DATA_BUFFER* dataToReceive = NULL;
    CM_ERROR error = CreateDataBuffer(&dataToReceive, sizeof(CM_HEADER)); 
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
        return -1;
    }

    CM_SIZE receivedByteCount = 0;
    error = ReceiveDataFromClient(Client, dataToReceive, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToReceive);
        return -1;
    }

    //check if entire structure arrived
    if (receivedByteCount != sizeof(CM_HEADER))
    {
        DestroyDataBuffer(dataToReceive);
        return -1;
    }

    CM_HEADER* tempHead = (CM_HEADER*)dataToReceive->DataBuffer;

    (*Header)->Cmd = tempHead->Cmd;
    (*Header)->Size = tempHead->Size;
    
    DestroyDataBuffer(dataToReceive);
    return 0;
}

/*
Sends input Buffer parameter to Client, according with the communication protocol, hence the Header
Returns 0 on success, -1 on error
*/
int SendBufferToClient(CM_SERVER_CLIENT* Client, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (Client == NULL || Header == NULL || Buffer == NULL)
    {
        //invalid parameters
        return -1;
    }
    CM_DATA_BUFFER* dataToSend = NULL;

    CM_ERROR error = CreateDataBuffer(&dataToSend, Max(Header->Size, (DWORD)sizeof(CM_HEADER)));
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        return -1;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)Header, sizeof(CM_HEADER));
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    CM_SIZE sendBytesCount = 0;
    error = SendDataToClient(Client, dataToSend, &sendBytesCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    //check if entire structure was sent
    if (sendBytesCount != sizeof(CM_HEADER))
    {
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)Buffer->DataBuffer, Header->Size);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    sendBytesCount = 0;
    error = SendDataToClient(Client, dataToSend, &sendBytesCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    //check if entire buffer was sent
    if (sendBytesCount != Header->Size)
    {
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    DestroyDataBuffer(dataToSend);
    return 0;
}

/*
Sends input Message parameter to Client, according with the communication protocol, hence the header
The function manages the creation of a CM_DATA_BUFFER and putting the Message inside the newly created buffer
Returns 0 on success, -1 on error
*/
int SendMessageToClient(CM_SERVER_CLIENT* Client, CM_HEADER* Header, char* Message)
{
    if (Client == NULL || Header == NULL || Message == NULL)
    {
        //invalid parameters
        return -1;
    }

    CM_DATA_BUFFER* dataBuffer = NULL;
    CM_SIZE size = (CM_SIZE)strlen(Message) + 1;
    Header->Size = size;
    CM_ERROR error = CreateDataBuffer(&dataBuffer, size);
    if (IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create data buffer!\n"));
        return -1;
    }

    error = CopyDataIntoBuffer(dataBuffer, (const CM_BYTE*)Message, size);
    if (IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to copy data into buffer!\n"));
        DestroyDataBuffer(dataBuffer);
        return -1;
    }

    int retVal = SendBufferToClient(Client, dataBuffer, Header);
    DestroyDataBuffer(dataBuffer);

    return retVal;
}

/*
Registers user that is found in input Buffer. If any error occurs, send corresponding error to client
Returns 0 on success, -1 if error
*/
int RegisterUser(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    //make sure no user is connected
    WaitForSingleObject(gMutexClients, INFINITE);
    if (ClientStruct->ConnectedStatus == 1)
    {
        //user is connected
        char toSend[] = "Error: User already logged in\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        ReleaseMutex(gMutexClients);
        return retVal;
    }
    ReleaseMutex(gMutexClients);

    //open file for read
    FILE* filePointer = NULL;
    if (fopen_s(&filePointer, "C:\\registration.txt", "rb") != 0)
    {
        _tprintf_s(TEXT("Unexpected error: Opening file to check register failed!\n"));
        return -1;
    }

    char* givenUserAndPass = malloc((strlen((char*)Buffer->DataBuffer) + 1) * sizeof(char));
    strcpy(givenUserAndPass, (const char*)Buffer->DataBuffer);  //givenUserAndPass now contains user pass combination

    char* user = strtok((char*)givenUserAndPass, " "); 
    char buffer[255]; //static buffer for reading from file

    //reading lines from file, first aquire SRWLock
    AcquireSRWLockShared(gRWLockRegFile);
    while (fgets(buffer, 255, filePointer) != NULL)
    {
        char* userComma = strtok(buffer, ",");  //user found in file
        int retVal = strcmp(userComma, user);

        if (retVal == 0)
        {
            //user already exists
            char toSend[] = "Error: Username already registered\n";  //error message to send to client

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
            ReleaseMutex(gMutexSend);

            ReleaseSRWLockShared(gRWLockRegFile);
            free(givenUserAndPass);
            fclose(filePointer);
            return retVal;
        }
    }
    ReleaseSRWLockShared(gRWLockRegFile);
    fclose(filePointer); //closing file for read
    free(givenUserAndPass);

    //now write user/pass
    givenUserAndPass = (char*)Buffer->DataBuffer;

    int sizeToWrite = (int)strlen((const char*)givenUserAndPass);

    //first i put comma instead of space
    for (int i = 0; givenUserAndPass[i]; i++)
    {
        if (givenUserAndPass[i] == ' ')
        {
            givenUserAndPass[i] = ',';
            break;
        }
    }

    //opening for write
    if (fopen_s(&filePointer, "C:\\registration.txt", "ab") != 0)
    {
        _tprintf_s(TEXT("Unexpected error: Opening file to write register failed!\n"));
        return -1;
    }

    AcquireSRWLockExclusive(gRWLockRegFile);
    //writing "user,pass"
    if (fwrite(givenUserAndPass, sizeof(char), sizeToWrite, filePointer) != sizeToWrite)
    {
        _tprintf_s(TEXT("Unexpected error: Writing to file failed!\n"));
        ReleaseSRWLockExclusive(gRWLockRegFile);
        fclose(filePointer);
        return -1;
    }
    ReleaseSRWLockExclusive(gRWLockRegFile);
    fclose(filePointer);
    
    //send succes to client
    char toSend[] = "Success\n";

    WaitForSingleObject(gMutexSend, INFINITE);
    int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
    ReleaseMutex(gMutexSend);

    return retVal;
}

/*
Login user that is found in input Buffer. If any error occurs, send corresponding error to client
Returns 0 on success, -1 if error
*/
int LoginUser(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    //check if user is not already logged in
    WaitForSingleObject(gMutexClients, INFINITE);
    if (ClientStruct->ConnectedStatus == 1)
    {
        char toSend[] = "Error: Another user already logged in\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        ReleaseMutex(gMutexClients);
        return retVal;
    }
    ReleaseMutex(gMutexClients);

    unsigned char* givenUserAndPass = Buffer->DataBuffer;
    char* userGiven = strtok((char*)givenUserAndPass, " ");
    char* passGiven = strtok(NULL, " ");

    char buffer[255]; //static buffer for reading from file

    //check if given user is not already connected
    WaitForSingleObject(gMutexClients, INFINITE);
    for (int i = 0; i < gNumberThreads; i++)
    {
        if (gClients[i] && gClients[i]->User != NULL)
        {
            int result = strcmp(userGiven, gClients[i]->User);
            if (gClients[i]->ConnectedStatus == 1 && result == 0)
            {
                char toSend[] = "Error: User already logged in\n";

                WaitForSingleObject(gMutexSend, INFINITE);
                int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
                ReleaseMutex(gMutexSend);

                ReleaseMutex(gMutexClients);
                return retVal;
            }
        }
    }
    ReleaseMutex(gMutexClients);

    //open file for read
    FILE* filePointer = NULL;
    if (fopen_s(&filePointer, "C:\\registration.txt", "rb") != 0)
    {
        _tprintf_s(TEXT("Unexpected error: Opening file to check register failed!\n"));
        return -1;
    }

    AcquireSRWLockShared(gRWLockRegFile);
    while (fgets(buffer, 255, filePointer) != NULL)
    {
        char* userFile = strtok(buffer, ",");    //i separate read user from password
        char* passFile = strtok(NULL, ",");

        int retVal1 = strcmp(userFile, userGiven);     
        int retVal2 = strcmp(passFile, passGiven);

        //both username and password match
        if (retVal1 == 0 && retVal2 == 0)
        {
            WaitForSingleObject(gMutexClients, INFINITE);
            ClientStruct->ConnectedStatus = 1;   //mark user as connected
            ClientStruct->User = malloc((strlen(userFile) + 1) * sizeof(char));
            if (ClientStruct->User == NULL)
            {
                //error alloc
                fclose(filePointer);
                ReleaseSRWLockShared(gRWLockRegFile);
                ClientStruct->ConnectedStatus = 0;
                return -1;
            }

            strcpy(ClientStruct->User, userFile);
            ReleaseMutex(gMutexClients);

            fclose(filePointer);
            ReleaseSRWLockShared(gRWLockRegFile);

            //send succes to client, user was logged in
            char toSend[] = "Success\n";

            WaitForSingleObject(gMutexSend, INFINITE);
            int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
            ReleaseMutex(gMutexSend);

            if (retVal != 0)
            {
                _tprintf_s(TEXT("Unexpected error: Error sending succes to client while logging in!\n"));
                return -1;
            }

            //now check if there are archived messages to show
            //archived messages will be kept in files containing the name of users
            //there will be 2 files, if we have user1 and user2, files will be user1user2.txt and user2user1.txt
            char* fileName = (char*)malloc((strlen(userGiven) + 5) * sizeof(char)); //4 characters ".txt" + 1 null terminator
            if (fileName == NULL)
            {
                //alloc error
                _tprintf_s(TEXT("Unexpected error: Failed to create fileName when logging in!\n"));
                return -1;
            }

            //build filename
            strcpy(fileName, userGiven);
            strcat(fileName, ".txt");

            if (fopen_s(&filePointer, fileName, "rb") != 0)
            {
                //file does not exists, so no archived messages
                free(fileName);
                return retVal;
            }

            char messageBuffer[255]; //255 is max for a line according to documentation
            while (fgets(messageBuffer, 255, filePointer) != NULL)
            {
                WaitForSingleObject(gMutexSend, INFINITE);
                retVal = SendMessageToClient(ClientStruct->Client, Header, messageBuffer);
                ReleaseMutex(gMutexSend);

                if (retVal != 0)
                {
                    _tprintf_s(TEXT("Unexpected error: Failed while sending an archived message!\n"));
                    free(fileName);
                    fclose(filePointer);
                    return retVal;
                }
            }
            fclose(filePointer);

            //now i need to convert fileName from standard char to wchar in order to use DeleteFile function due to the fact that the file will no longer be needed
            retVal = MultiByteToWideChar(
                CP_ACP,              
                0,            
                fileName, 
                -1, 
                NULL, 
                0
            );

            wchar_t* wstr = (wchar_t*)malloc(sizeof(wchar_t) * retVal);
            if (wstr == NULL)
            {
                free(fileName);
                _tprintf_s(TEXT("Unexpected error: Failed to create wchar_t buffer!\n"));
                return -1;
            }

            retVal1 = MultiByteToWideChar(
                CP_ACP, 
                0, 
                fileName, 
                -1, 
                wstr, 
                retVal);

            if (DeleteFile(wstr) == 0)
            {
                free(wstr);
                free(fileName);
                _tprintf_s(TEXT("Unexpected error: Failed to delete temporary file, error code %d \n"), GetLastError());
                return -1;
            }

            free(wstr);
            free(fileName);

            return 0;
        }
    }
    fclose(filePointer);
    ReleaseSRWLockShared(gRWLockRegFile);
    
    //if invalid user/pass notify client
    char toSend[] = "Error: Invalid username/password combination\n";

    WaitForSingleObject(gMutexSend, INFINITE);
    int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
    ReleaseMutex(gMutexSend);

    return retVal;
}

/*
Performs logout command for current user. If any error occurs, send corresponding error to client
Returns 0 on success, -1 if error
*/
int LogoutUser(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    WaitForSingleObject(gMutexClients, INFINITE);
    if (ClientStruct->ConnectedStatus == 0)
    {
        //no user is connected
        char toSend[] = "Error: No user currently logged in\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        ReleaseMutex(gMutexClients);
        return retVal;
    }
    else
    {
        //logout user
        free(ClientStruct->User);
        ClientStruct->User = NULL;
        ClientStruct->ConnectedStatus = 0;
        ReleaseMutex(gMutexClients);

        char toSend[] = "Success\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        return retVal;
    }
}

/*
Lists all connected users and sends them to current user. If any error occurs, send corresponding error to client
Returns 0 on success, -1 if error
*/
int ListUsers(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    char* toSend = NULL;       //here will be all users
    
    WaitForSingleObject(gMutexClients, INFINITE);
    for (int i = 0; i < gNumberThreads; i++)
    {
        if (gClients[i] == NULL || gClients[i]->ConnectedStatus == 2 || gClients[i]->ConnectedStatus == 0)
        {
            //skip unlogged or unexistent clients
            continue;
        }
        else
        {
            if (toSend == NULL)
            {
                //alloc buffer
                toSend = (char*)malloc((strlen(gClients[i]->User) + 2) * sizeof(char)); //+1 for NULL and +1 for \n
                if (toSend == NULL)
                {
                    //alloc error
                    ReleaseMutex(gMutexClients);
                    return -1;
                }
                strcpy(toSend, gClients[i]->User);
                strcat(toSend, "\n");
            }
            else
            {
                //realloc buffer and just append username and newline
                char* temp = NULL;
                temp = realloc(toSend, (strlen(toSend) + strlen(gClients[i]->User) + 2) * sizeof(char));
                if (temp == NULL)
                {
                    //realloc error
                    free(toSend);
                    ReleaseMutex(gMutexClients);
                    return -1;
                }
                toSend = temp;
                strcat(toSend, gClients[i]->User);
                strcat(toSend, "\n");
            }
        }
    }
    ReleaseMutex(gMutexClients);

    if (toSend == NULL)
    {
        //no user connected, notify client
        char err[] = "Unexpected error: No user is logged in at the moment!\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, err);
        ReleaseMutex(gMutexSend);

        return retVal;
    }

    WaitForSingleObject(gMutexSend, INFINITE);
    int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
    ReleaseMutex(gMutexSend);

    free(toSend);
    return retVal;
}

/*
Broadcasts message inside input Buffer to all clients, including those that do not have a user logged in. If any error occurs, send corresponding error to broadcasting client
Returns 0 on success, -1 if error
*/
int BroadcastFromUser(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    WaitForSingleObject(gMutexClients, INFINITE);
    if (ClientStruct->ConnectedStatus == 0)
    {
        //error, no current user logged in
        char toSend[] = "Error: No user currently logged in\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        ReleaseMutex(gMutexClients);
        return retVal;
    }
    ReleaseMutex(gMutexClients);

    char* broadcastMessage = NULL;
    int sizeWithoutUser = (int)strlen("Broadcast from : ") + (int)strlen((char*)Buffer->DataBuffer) + 1;

    WaitForSingleObject(gMutexClients, INFINITE);
    sizeWithoutUser += (int)strlen(ClientStruct->User);
    ReleaseMutex(gMutexClients);

    broadcastMessage = (char*)malloc(sizeWithoutUser * sizeof(char));
    if (broadcastMessage == NULL)
    {
        //alloc error
        return -1;
    }

    //now build entire message
    strcpy(broadcastMessage, "Broadcast from ");

    WaitForSingleObject(gMutexClients, INFINITE);
    strcat(broadcastMessage, ClientStruct->User);
    strcat(broadcastMessage, ": ");
    strcat(broadcastMessage, (const char*)Buffer->DataBuffer);

    for (int i = 0; i < gNumberThreads; i++)
    {
        //if user exists and is connected, does not matter if logged in, and not same as current, send broadcast message
        if (gClients[i] != NULL && gClients[i]->ConnectedStatus != 2 && i != ClientStruct->Position)
        {
            WaitForSingleObject(gMutexSend, INFINITE);
            int retVal = SendMessageToClient(gClients[i]->Client, Header, broadcastMessage);
            ReleaseMutex(gMutexSend);

            if (retVal != 0)
            {
                //error sending to some user
                _tprintf_s(TEXT("Unexpected error: Failed to send broadcast message to a client!\n"));
                ReleaseMutex(gMutexClients);
                free(broadcastMessage);
                return -1;
            }
        }
    }
    ReleaseMutex(gMutexClients);
    free(broadcastMessage);

    //send success to client
    char toSend[] = "Success\n";

    WaitForSingleObject(gMutexSend, INFINITE);
    int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
    ReleaseMutex(gMutexSend);

    return retVal;
}

/*
Adds input buffer Message into history file. Sender and Receiver parameters are usernames of clients that are communicating
Returns 0 on success, -1 if error
*/
int AddToHistoryFile(char* Sender, char* Receiver, char* Message)
{
    if (Sender == NULL || Receiver == NULL || Message == NULL)
    {
        //invalid parameters
        return -1;
    }

    //modify structure of message, remove "Message " and make 'f' into 'F'
    char* modifiedMessage = (char*)malloc((strlen(Message) + 1) * sizeof(char));
    if (modifiedMessage == NULL)
    {
        //alloc error
        _tprintf_s(TEXT("Unexpected error: Failed to create temporary message buffer!\n"));
        return -1;
    }

    int i = 0;
    for (i = 0; Message[i]; i++)
    {
        if (Message[i] == 'f')
        {
            break;
        }
    }
    strcpy(modifiedMessage, Message + i);
    modifiedMessage[0] = 'F';

    //i will write to 2 different files, temporary solution, will change if time allows
    char* file1 = (char*)malloc((strlen(Sender) + strlen(Receiver) + 5) * sizeof(char)); //4 characters ".txt" and 1 null terminator
    if (file1 == NULL)
    {
        //alloc error
        free(modifiedMessage);
        return -1;
    }

    char* file2 = (char*)malloc((strlen(Sender) + strlen(Receiver) + 5) * sizeof(char)); //4 characters ".txt" and 1 null terminator
    if (file2 == NULL)
    {
        //alloc error
        free(file1);
        free(modifiedMessage);
        return -1;
    }

    FILE* filePointer1 = NULL;
    FILE* filePointer2 = NULL;

    strcpy(file1, Sender);
    strcat(file1, Receiver);
    strcat(file1, ".txt");

    strcpy(file2, Receiver);
    strcat(file2, Sender);
    strcat(file2, ".txt");

    if (fopen_s(&filePointer1, file1, "ab") != 0)
    {
        free(file1);
        free(modifiedMessage);
        free(file2);
        _tprintf_s(TEXT("Unexpected error: Failed to open file1 to write history!\n"));
        return -1;
    }
    if (fwrite(modifiedMessage, sizeof(char), (strlen(modifiedMessage) * sizeof(char)), filePointer1) != (strlen(modifiedMessage) * sizeof(char)))
    {
        free(file1);
        free(file2);
        free(modifiedMessage);
        fclose(filePointer1);
        _tprintf_s(TEXT("Unexpected error: Failed to write message to file1!\n"));
        return -1;
    }
    fclose(filePointer1);


    if (fopen_s(&filePointer2, file2, "ab") != 0)
    {
        free(file1);
        free(file2);
        free(modifiedMessage);
        _tprintf_s(TEXT("Unexpected error: Failed to open file1 to write history!\n"));
        return -1;
    }
    if (fwrite(modifiedMessage, sizeof(char), (strlen(modifiedMessage) * sizeof(char)), filePointer2) != (strlen(modifiedMessage) * sizeof(char)))
    {
        free(file1);
        free(file2);
        free(modifiedMessage);
        fclose(filePointer2);
        _tprintf_s(TEXT("Unexpected error: Failed to write message to file1!\n"));
        return -1;
    }
    fclose(filePointer2);


    free(file1);
    free(file2);
    free(modifiedMessage);

    return 0;
}

/*
Sends message to given user in input Buffer. Message is also in input Buffer. Current client is the sender. If any error occurs, send message to sender client
Returns 0 on success, -1 if error
*/
int SendMessageFromUser(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    WaitForSingleObject(gMutexClients, INFINITE);
    if (ClientStruct->ConnectedStatus == 0)
    {
        //error, no current user logged in
        char toSend[] = "Error: No user currently logged in\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        ReleaseMutex(gMutexClients);
        return retVal;
    }
    ReleaseMutex(gMutexClients);

    char staticUser[255];   //static buffer used to store user to send message

    int i = 0;
    char* givenBuffer = (char*)Buffer->DataBuffer;
    for (i = 0; givenBuffer[i]; i++)
    {
        if (givenBuffer[i] != ' ')
        {
            staticUser[i] = givenBuffer[i];
        }
        else
        {
            //when i found space it means that i have the full username inside staticUser
            break;
        }
    }

    staticUser[i] = '\0';  //add null terminator
    char* rawMessage = (char*)(Buffer->DataBuffer + i + 1);  //put entire message in rawMessage, without username

    char* message = NULL;
    int size = (int)strlen("Message from : ") + (int)strlen(rawMessage) + 1;

    //start building full message with prefix
    WaitForSingleObject(gMutexClients, INFINITE);
    size += (int)strlen(ClientStruct->User);
    ReleaseMutex(gMutexClients);

    message = (char*)malloc(size * sizeof(char));
    if (message == NULL)
    {
        //alloc error
        _tprintf_s(TEXT("Unexpected error: Failed to create message to send to other user!\n"));
        return -1;
    }

    //builiding prefix
    strcpy(message, "Message from ");
    WaitForSingleObject(gMutexClients, INFINITE);
    strcat(message, ClientStruct->User);
    ReleaseMutex(gMutexClients);
    strcat(message, ": ");
    strcat(message, rawMessage);

    //open file for read
    FILE* filePointer = NULL;
    if (fopen_s(&filePointer, "C:\\registration.txt", "rb") != 0)
    {
        _tprintf_s(TEXT("Unexpected error: Opening file to check register failed!\n"));
        free(message);
        return -1;
    }

    char buffer[255]; //buffer used to read from file
    AcquireSRWLockShared(gRWLockRegFile);
    while (fgets(buffer, 255, filePointer))
    {
        char* userComma = strtok(buffer, ",");
        int retVal = strcmp(userComma, staticUser);
        if (retVal == 0)
        {
            //found user
            fclose(filePointer);
            ReleaseSRWLockShared(gRWLockRegFile);

            WaitForSingleObject(gMutexClients, INFINITE);
            for (i = 0; i < gNumberThreads; i++)
            {
                if (gClients[i] != NULL && gClients[i]->User != NULL)
                {
                    if (strcmp(gClients[i]->User, (const char*)staticUser) == 0 && i != ClientStruct->Position)
                    {
                        //found user to send message

                        //add message to history file
                        retVal = AddToHistoryFile(ClientStruct->User, staticUser, message);
                        if (retVal != 0)
                        {
                            //error writing to file for history
                            _tprintf_s(TEXT("Unexpected error: Failed to write to history file!\n"));
                            ReleaseMutex(gMutexClients);
                            free(message);
                            return -1;
                        }

                        //send message to recipient

                        WaitForSingleObject(gMutexSend, INFINITE);
                        int retVal1 = SendMessageToClient(gClients[i]->Client, Header, message);
                        ReleaseMutex(gMutexSend);

                        ReleaseMutex(gMutexClients);
                        free(message);

                        //send succes to sender
                        char toSend[] = "Success\n";

                        WaitForSingleObject(gMutexSend, INFINITE);
                        int retVal2 = SendMessageToClient(ClientStruct->Client, Header, toSend);
                        ReleaseMutex(gMutexSend);
                        
                        return retVal1 || retVal2;
                    }
                }
            }
            ReleaseMutex(gMutexClients);

            //user not connected, still, save message in temp file(userName.txt) to send when user logs in
            char* fileName = NULL;
            fileName = (char*)malloc((strlen(staticUser) + 5) * sizeof(char));  // 4 characters ".txt" and 1 for null terminator
            if (fileName == NULL)
            {
                //alloc error
                _tprintf_s(TEXT("Unexpected error: Failed to create fileName!\n"));
                free(message);
                return -1;
            }
            strcpy(fileName, staticUser);
            strcat(fileName, ".txt");

            filePointer = NULL;
            if (fopen_s(&filePointer, fileName, "ab") != 0)
            {
                _tprintf_s(TEXT("Unexpected error: Opening file to write message failed!\n"));
                free(message);
                return -1;
            }

            if (fwrite(message, sizeof(char), strlen(message) * sizeof(char), filePointer) != strlen(message) * sizeof(char))
            {
                _tprintf_s(TEXT("Unexpected error: Writing message to file failed!\n"));
                free(message);
                fclose(filePointer);
                return -1;
            }
            fclose(filePointer);
            free(fileName);

            //add to history file even if user is not connected
            retVal = AddToHistoryFile(ClientStruct->User, staticUser, message);
            if (retVal != 0)
            {
                //error writing to file for history
                _tprintf_s(TEXT("Unexpected error: Failed to write to history file!\n"));
                free(message);
                return -1;
            }
            free(message);

            //send succes to client
            char toSend[] = "Success\n";

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
            ReleaseMutex(gMutexSend);

            return retVal;
        }
    }
    fclose(filePointer);
    ReleaseSRWLockShared(gRWLockRegFile);

    //no user found, send error to sender client
    free(message);
    char toSend[] = "Error: No such user\n";

    WaitForSingleObject(gMutexSend, INFINITE);
    int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
    ReleaseMutex(gMutexSend);
    
    return retVal;
}

/*
Function validates if the user found in input Buffer is active and exists and if so, sends ok to client to start sending file
If any error occurs, send corresponding error to sender client
Returns:
2 if file can not be sent
0 on succes, file can be sent 
-1 if error
*/
int ReceiveFileFromUser(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invlid parameters
        return -1;
    }

    WaitForSingleObject(gMutexClients, INFINITE);
    if (ClientStruct->ConnectedStatus != 1)
    {
        //no user is connected
        char errMsg[] = "Error: No user currently logged in\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, errMsg);
        ReleaseMutex(gMutexSend);
        
        ReleaseMutex(gMutexClients);
        if (retVal != 0)
        {
            _tprintf_s(TEXT("Unexpected error: Failed to send no user is logged in to client when send file!\n"));
            return -1;
        }

        return 2;
    }
    ReleaseMutex(gMutexClients);

    char* copyMsg = (char*)malloc((strlen((const char*)Buffer->DataBuffer) + 1) * sizeof(char));
    if (copyMsg == NULL)
    {
        //error alloc copyMsg
        _tprintf_s(TEXT("Unexpected error: Failed to alloc copyMsg!\n"));
        return -1;
    }
    strcpy(copyMsg, (const char*)Buffer->DataBuffer);  //make copy of buffer

    char* user = strtok(copyMsg, " ");

    //now it's time to validate user and send message back to client

    //open file for read
    FILE* filePointer = NULL;
    if (fopen_s(&filePointer, "C:\\registration.txt", "rb") != 0)
    {
        _tprintf_s(TEXT("Unexpected error: Opening file to check register failed!\n"));
        free(copyMsg);
        return -1;
    }

    char buffer[255]; //static buffer for reading from file

    //reading lines from file
    AcquireSRWLockShared(gRWLockRegFile);
    while (fgets(buffer, 255, filePointer) != NULL)
    {
        char* userComma = strtok(buffer, ",");
        int retVal = strcmp(userComma, user);
        if (retVal == 0)
        {
            //user exists
            ReleaseSRWLockShared(gRWLockRegFile);
            fclose(filePointer);
            
            //now check if user is connected
            WaitForSingleObject(gMutexClients, INFINITE);
            for (int i = 0; i < gNumberThreads; i++)
            {
                if (gClients[i] != NULL && gClients[i]->User != NULL)
                {
                    if (strcmp(gClients[i]->User, user) == 0)
                    {
                        //found user and it is active, send ok to client
                        ReleaseMutex(gMutexClients);

                        char okMsg[] = "Success\n";

                        WaitForSingleObject(gMutexSend, INFINITE);
                        retVal = SendMessageToClient(ClientStruct->Client, Header, okMsg);
                        ReleaseMutex(gMutexSend);

                        free(copyMsg);
                        if (retVal == 0)
                        {
                            return 0;
                        }
                        else
                        {
                            return 2;
                        }
                    }
                }
            }
            ReleaseMutex(gMutexClients);

            //user is not active
            char errNotActive[] = "Error: User not active\n";

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(ClientStruct->Client, Header, errNotActive);
            ReleaseMutex(gMutexSend);

            if (retVal == 0)
            {
                free(copyMsg);
                return 2; 
            }
            else
            {
                free(copyMsg);
                return -1;
            }
            
        }
    }
    ReleaseSRWLockShared(gRWLockRegFile);
    fclose(filePointer); //closing file for read
    free(copyMsg);

    //send NO such user to client
    char errNoSuchUser[] = "Error: No such user\n";

    WaitForSingleObject(gMutexSend, INFINITE);
    int retVal = SendMessageToClient(ClientStruct->Client, Header, errNoSuchUser);
    ReleaseMutex(gMutexSend);
    
    if (retVal == 0)
    {
        return 2; 
    }
    else
    {
        return -1;
    }
}

/*
Sends last count messages with given user. If no messages found, send \n. Count and user are found in input Buffer
If any error occurs, send corresponding error to sender client
Returns 0 on success, -1 if error
*/
int HistoryForUser(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header) 
{
    if (ClientStruct == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    char* otherUser = strtok((char*)Buffer->DataBuffer, " ");
    char* asciiCount = strtok(NULL, " ");

    int count = atoi(asciiCount);  //make count integer

    WaitForSingleObject(gMutexClients, INFINITE);
    if (ClientStruct->ConnectedStatus != 1)
    {
        //no user is logged in
        ReleaseMutex(gMutexClients);
        char toSend[] = "Error: No user currently logged in\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        return retVal;
    }

    char* currentUser = (char*)malloc((strlen(ClientStruct->User) + 1) * sizeof(char));
    if (currentUser == NULL)
    {
        //alloc error
        return -1;
    }
    strcpy_s(currentUser, strlen(ClientStruct->User) + 1, ClientStruct->User);
    ReleaseMutex(gMutexClients);

    FILE* filePointer = NULL;
    //check if user exists
    if (fopen_s(&filePointer, "C:\\registration.txt", "rb") != 0)
    {
        _tprintf_s(TEXT("Unexpected error: Failed to open registration file for register!\n"));
        free(currentUser);
        return -1;
    }
    char staticBuffer[255];
    int found = 0; //flag to signal that user was found in registration file

    AcquireSRWLockShared(gRWLockRegFile);
    while (fgets(staticBuffer, 255, filePointer) != NULL)
    {
        char* fileUser = strtok(staticBuffer, ",");
        if (strcmp(fileUser, otherUser) == 0)
        {
            //we found user, all good, signal the flag
            found = 1;
            break;
        }
    }
    ReleaseSRWLockShared(gRWLockRegFile);
    fclose(filePointer);

    if (found == 0)
    {
        //no such user
        char toSend[] = "Error: No such user\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        free(currentUser);
        return retVal;
    }

    char* fileName = (char*)malloc((strlen(currentUser) + strlen(otherUser) + 5) * sizeof(char)); //4 for ".txt" and 1 null terminator 
    if (fileName == NULL)
    {
        //alloc error
        free(currentUser);
        return -1;
    }

    strcpy(fileName, currentUser);
    strcat(fileName, otherUser);
    strcat(fileName, ".txt");

    if (fopen_s(&filePointer, fileName, "rb") != 0)
    {
        //no messages, just send \n
        char toSend[] = "\n";

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, toSend);
        ReleaseMutex(gMutexSend);

        free(currentUser);
        return retVal;
    }
    free(currentUser); //i do not need current username anymore


    fseek(filePointer, 0, SEEK_END); //put cursor at end of file, we count lines backwards
    int pos = ftell(filePointer);
    int nrLines = 0;
    while (pos) 
    {
        fseek(filePointer, --pos, SEEK_SET); 
        if (fgetc(filePointer) == '\n') 
        {
            //another line found
            nrLines += 1;
            if (nrLines == count + 1)
            {
                break;
            }
        }
    }
    if (pos == 0)
    {
        //we just need to put seek at begin of file
        fseek(filePointer, 0, SEEK_SET);
    }

    //just read the lines and send them to client
    while (fgets(staticBuffer, 255, filePointer) != NULL)
    {
        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendMessageToClient(ClientStruct->Client, Header, staticBuffer);
        ReleaseMutex(gMutexSend);

        if (retVal != 0)
        {
            _tprintf_s(TEXT("Unexpected error: Failed to send line from history file to client!\n"));
            free(fileName);
            fclose(filePointer);
            return -1;
        }
    }
    fclose(filePointer);
    free(fileName);

    return 0;
}

/*
Function receives Buffer and Header from client and then assigns task to corresponding function
Returns:
0 on success
1 if client just exited
8 if client is about to send a file and the Buffer was valid
-1 if error
*/
int Decide(CLIENT* ClientStruct, CM_DATA_BUFFER* Buffer, CM_HEADER* Header)
{
    if (Buffer == NULL || ClientStruct == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    int retVal = -1;
    char exitMsg[] = "DIE";  //message to announce client that it needs to shutdown

    switch (Header->Cmd)
    {
    case 1:
        WaitForSingleObject(gMutexSend, INFINITE);
        retVal = SendBufferToClient(ClientStruct->Client, Buffer, Header);
        ReleaseMutex(gMutexSend);

        char* toPrint = NULL;
        toPrint = (char*)Buffer->DataBuffer;
        _tprintf_s(TEXT("%hs"), toPrint);
        return retVal;
    case 2:
        retVal = RegisterUser(ClientStruct, Buffer, Header);
        return retVal;
    case 3:
        retVal = LoginUser(ClientStruct, Buffer, Header);
        return retVal;
    case 4:
        retVal = LogoutUser(ClientStruct, Buffer, Header);
        return retVal;
    case 5:
        retVal = SendMessageFromUser(ClientStruct, Buffer, Header);
        return retVal;
    case 6:
        retVal = BroadcastFromUser(ClientStruct, Buffer, Header);
        return retVal;
    case 7:
        //first time i receive commmand to start receive file
        //just validate and send ok/error
        retVal = ReceiveFileFromUser(ClientStruct, Buffer, Header);
        if (retVal != 0 && retVal != 2)
        {
            //error
            return -1;
        }
        else if(retVal == 0)
        {
            //signal ServeClient that he is about to receive a file and pass it on to another client
            return 8;
        }
        else
        {
            return 0;
        }
    case 8:
        retVal = ListUsers(ClientStruct, Buffer, Header);
        return retVal;
    case 9:   
        retVal = SendMessageToClient(ClientStruct->Client, Header, exitMsg);
        return 1;
    case 10:
        retVal = HistoryForUser(ClientStruct, Buffer, Header);
        return retVal;
    default:
        return retVal;
    }
}

/*
This function will be "main loop" for every connected client. Function must be called in a separate thread
Receives data from client according to protocol and decide at each iteration what to do with received data
Also sends back data to user accordingly, with help of other functions
*/
int ServeClient(CLIENT *newClientStruct)
{
    CM_ERROR error;
    int retVal = -1;
    
    CM_DATA_BUFFER* dataToReceive = NULL;  //buffer to receive data
    CM_HEADER* header = NULL;              //received header
    
    CM_SERVER_CLIENT* clientToSend = NULL;    //here is the user to receive file, will be allocated for each file send 
   
    while (TRUE)
    {
        header = (CM_HEADER*)malloc(sizeof(CM_HEADER));
        retVal = ReceiveHeader(newClientStruct->Client, &header);
        if (retVal != 0)
        {
            char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
            ReleaseMutex(gMutexSend);

            free(header);
            AbandonClient(newClientStruct->Client);
            return -1;
        }

        //now i create the buffer where actual data will be recieved
        error = CreateDataBuffer(&dataToReceive, header->Size);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
            char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
            ReleaseMutex(gMutexSend);

            AbandonClient(newClientStruct->Client);
            return -1;
        }

        CM_SIZE receivedByteCount = 0;
        
        error = ReceiveDataFromClient(newClientStruct->Client, dataToReceive, &receivedByteCount);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
            char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
            ReleaseMutex(gMutexSend);

            free(header);
            DestroyDataBuffer(dataToReceive);
            AbandonClient(newClientStruct->Client);
            return -1;
        }
        if (receivedByteCount != header->Size)
        {
            //not enough data arrived
            _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed to receive all data\n"));
            char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
            ReleaseMutex(gMutexSend);

            free(header);
            DestroyDataBuffer(dataToReceive);
            AbandonClient(newClientStruct->Client);
            return -1;
        }

        if (header->Cmd == 88)
        {
            //send file to client, that's all, buffer just contains part of file
            if (clientToSend != NULL)
            {
                WaitForSingleObject(gMutexSend, INFINITE);
                retVal = SendBufferToClient(clientToSend, dataToReceive, header);
                ReleaseMutex(gMutexSend);

                if (retVal != 0)
                {
                    _tprintf_s(TEXT("Unexpected error: Failed to send part of file to client!\n"));
                    char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

                    WaitForSingleObject(gMutexSend, INFINITE);
                    retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
                    ReleaseMutex(gMutexSend);

                    free(header);
                    DestroyDataBuffer(dataToReceive);
                    AbandonClient(newClientStruct->Client);
                    return -1;
                }
            }
        }
        else if (header->Cmd == 75)
        {
            //file was passed, now just cleanup and send to that client name of sender
            WaitForSingleObject(gMutexClients, INFINITE);
            char* tempUserName = NULL;
            tempUserName = (char*)malloc((strlen(newClientStruct->User) + 1) * sizeof(char));
            if (tempUserName == NULL)
            {
                //alloc errror
                _tprintf_s(TEXT("Unexpected error: Failed to alloc copy of username when ending to send file\n"));
                char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

                WaitForSingleObject(gMutexSend, INFINITE);
                retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
                ReleaseMutex(gMutexSend);

                clientToSend = NULL;
                DestroyDataBuffer(dataToReceive);
                free(header);
                AbandonClient(newClientStruct->Client);
                return -1;
            }
            strcpy(tempUserName, newClientStruct->User);
            ReleaseMutex(gMutexClients);

            WaitForSingleObject(gMutexSend, INFINITE);
            retVal = SendMessageToClient(clientToSend, header, tempUserName);
            ReleaseMutex(gMutexSend);

            if (retVal != 0)
            {
                _tprintf_s(TEXT("Unexpected error: Failed to send succes to receiver!\n"));
                char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

                WaitForSingleObject(gMutexSend, INFINITE);
                retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
                ReleaseMutex(gMutexSend);

                clientToSend = NULL;
                DestroyDataBuffer(dataToReceive);
                free(header);
                AbandonClient(newClientStruct->Client);
                return retVal;
            }
            free(tempUserName);
            clientToSend = NULL;
        }
        else
        {
            retVal = Decide(newClientStruct, dataToReceive, header);
            if (retVal == -1)
            {
                //error
                char toSend[] = "Unexpected error: Something wrong happened at the server\n";

                WaitForSingleObject(gMutexSend, INFINITE);
                retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
                ReleaseMutex(gMutexSend);

                return retVal;
            }
            else if (retVal == 1)
            {
                //exit case
                DestroyDataBuffer(dataToReceive);
                free(header);
                WaitForSingleObject(gMutexClients, INFINITE);
                if (newClientStruct->ConnectedStatus == 1)
                {
                    //free username buffer if client was logged in
                    free(newClientStruct->User);
                }
                newClientStruct->ConnectedStatus = 2;  //mark client struct as unconnected 
                newClientStruct->User = NULL;
                ReleaseMutex(gMutexClients);
                AbandonClient(newClientStruct->Client);
                break;
            }
            else if (retVal == 8)
            {
                //prepare to receive file and pass it on
                char* userName = strtok((char*)dataToReceive->DataBuffer, " ");
                char* fileName = strtok(NULL, " ");

                WaitForSingleObject(gMutexClients, INFINITE);
                for (int i = 0; i < gNumberThreads; i++)
                {
                    if (gClients[i] && gClients[i]->User)
                    {
                        if (strcmp(gClients[i]->User, userName) == 0)
                        {
                            //found user
                            clientToSend = gClients[i]->Client;
                            break;
                        }
                    }
                }
                ReleaseMutex(gMutexClients);

                header->Cmd = 55; //so name of file will be sent

                WaitForSingleObject(gMutexSend, INFINITE);
                retVal = SendMessageToClient(clientToSend, header, fileName);
                ReleaseMutex(gMutexSend);
                //treat error

                if (retVal != 0)
                {
                    _tprintf_s(TEXT("Unexpected error: Failed to send to receiver that file is about to come(cmd=55)!\n"));
                    char toSend[] = "Unexpected error: Some error occured, you will be dissconected\n";

                    WaitForSingleObject(gMutexSend, INFINITE);
                    retVal = SendMessageToClient(newClientStruct->Client, header, toSend);
                    ReleaseMutex(gMutexSend);

                    clientToSend = NULL;
                    DestroyDataBuffer(dataToReceive);
                    free(header);
                    AbandonClient(newClientStruct->Client);
                    return retVal;
                }
            }
        }

        DestroyDataBuffer(dataToReceive);
        free(header);   
    }
    return 0;
}

/*
Function to be called by separate thread, this function just calls ServeClient with given parameter
*/
DWORD ClientPoolFunc(PVOID pParameter)
{
    if (pParameter == NULL)
    {
        //invalid parameter
        return 1;
    }

    CLIENT *client = (CLIENT*)pParameter;
    return ServeClient(client);
}

/*
Function sends back response to client after connection was established
If there is room on server or not, client will be notified
Response is in input Message parameter
*/
void SendResponseToClient(CM_SERVER_CLIENT* Client, char* Message)
{
    if (Client == NULL || Message == NULL)
    {
        //invalid parameters
        return;
    }

    CM_DATA_BUFFER* buffer = NULL;
    CM_ERROR error = CreateDataBuffer(&buffer, (CM_SIZE)strlen(("Error: maximum concurrent connection count reached\n")) + 1);  //i will only use buffer without header, so buffer will have this fixed size, no matter the response
    if (IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Error creating data buffer\n"));
        AbandonClient(Client);
        return;
    }

    error = CopyDataIntoBuffer(buffer, (const CM_BYTE*)Message, (CM_SIZE)strlen(("Error: maximum concurrent connection count reached\n")) + 1);
    if (IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Error putting data into buffer while refusing client\n"));
        AbandonClient(Client);
        DestroyDataBuffer(buffer);
        return;
    }

    CM_SIZE sentBytes = 0;
    error = SendDataToClient(Client, buffer, &sentBytes);
    if (IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Error sending login response to client!\n"));
        AbandonClient(Client);
        DestroyDataBuffer(buffer);
        return;
    }
    
    DestroyDataBuffer(buffer);
}

/*
Handler for CTRL-C event, only exit situation for server apart from error
Will perform cleanup and free resources
*/
BOOL WINAPI CleanupHandler(DWORD Signal) 
{
    if (Signal == CTRL_C_EVENT)
    {
        if (NULL != gPool)
        {
            THreadPoolDestroy(gPool);
            gPool = NULL;
        }

        CloseHandle(gMutexClients);
        CloseHandle(gMutexSend);

        if (NULL != gClients)
        {
            for (int i = 0; i < gNumberThreads; i++)
            {
                if (gClients[i] != NULL)
                {
                    free(gClients[i]);
                }
            }
            free(gClients);
        }

        DestroyServer(gServer);
        UninitCommunicationModule();
    }
        
    return FALSE;  
}

/*
Main function of server
*/
int _tmain(int argc, TCHAR* argv[])
{
    if (SetConsoleCtrlHandler(CleanupHandler, TRUE) == FALSE) 
    {
        _tprintf_s(TEXT("Unexpected error: Could not set CTRL-C Event handler!\n"));
        return 1;
    }

    int ret;

    gPool = NULL;
    ret = -1;

    if (argc != 2)
    {
        //invalid number of param
        _tprintf_s(TEXT("Unexpected error: invalid number of parameters!\n"));
        return -1;
    }

    for (int i = 0; argv[1][i]; i++)
    {
        if (!isdigit(argv[1][i]))
        {
            _tprintf_s(TEXT("Error: invalid maximum number of connections\n"));
            return -1;
        }
    }
    _tprintf_s(TEXT("Success\n"));

    gNumberThreads = _wtoi(argv[1]);
    //EnableCommunicationModuleLogger();

    CM_ERROR error = InitCommunicationModule();
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: InitCommunicationModule failed with err-code=0x%X!\n"), error);
        return -1;
    }

    gServer = NULL;
    error = CreateServer(&gServer);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CreateServer failed with err-code=0x%X!\n"), error);
        goto cleanup;
    }

    int retVal = ThreadPoolCreate(&gPool, gNumberThreads);
    if (retVal != 0)
    {
        _tprintf_s(TEXT("Unexpected error: ThreadPoolCreate failed: %d!\n"), retVal);
        goto cleanup;
    }

    //creating and preparing vector of CLIENT, will represent all clients connected.If element in vector is null, no client connected
    //no clients will be connected initially, no particular order will be kept in the vector
    gClients = (CLIENT**)malloc(sizeof(CLIENT*) * gNumberThreads);
    if (gClients == NULL)
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create client array!\n"));
        goto cleanup;
    }
   
    for (int i = 0; i < gNumberThreads; i++) //no clients initially
    {
        gClients[i] = NULL;
    }

    //initialise SRWLock for reg file
    gRWLockRegFile = (PSRWLOCK)malloc(sizeof(SRWLOCK));
    if (gRWLockRegFile == NULL)
    {
        //alloc error
        _tprintf_s(TEXT("Unexpected error: Failed to alloc SRWLock for registration file!\n"));
        goto cleanup;
    }
    InitializeSRWLock(gRWLockRegFile);

    gMutexClients = CreateMutex(NULL, FALSE, NULL);   //mutex for client array
    gMutexSend = CreateMutex(NULL, FALSE, NULL);      //mutex for sending data

    if (gMutexClients == NULL)
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create mutex for client array!\n"));
        goto cleanup;
    }

    if (gMutexSend == NULL)
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create mutex for data send!\n"));
        goto cleanup;
    }

    while (TRUE)
    {
        CM_SERVER_CLIENT* newClient = NULL; 
        error = AwaitNewClient(gServer, &newClient);

        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: AwaitNewClient failed with err-code=0x%X!\n"), error);
            goto cleanup;
        }
        
        // Call ServeClient in a separate thread. Synchronization is managed automatically in ThreadPool module.
        CLIENT* newClientStruct = NULL;
        newClientStruct = (CLIENT*)malloc(sizeof(CLIENT));
        if (newClientStruct == NULL)
        {
            _tprintf_s(TEXT("Unexpected error: Failed to create client struct!\n"));
            AbandonClient(newClient);
            goto cleanup;
        }

        newClientStruct->Client = newClient;
        if (ThreadPoolApply(gPool, ClientPoolFunc, (PVOID)newClientStruct) == 1)
        {
            //send err, no more space on server
            char msg[] = "Error: maximum concurrent connection count reached\n";
            SendResponseToClient(newClient, msg);
            AbandonClient(newClient);
        }
        else
        {
            //send ok to client
            WaitForSingleObject(gMutexClients, INFINITE);
            for (int i = 0; i < gNumberThreads; i++)
            {
                if (gClients[i] == NULL || gClients[i]->ConnectedStatus == 2)
                {
                        gClients[i] = newClientStruct;
                        gClients[i]->ConnectedStatus = 0;
                        gClients[i]->User = NULL;
                        gClients[i]->Position = i;
                        ReleaseMutex(gMutexClients);
                        break;
                }
            }
            char msg[] = "OK!";
            SendResponseToClient(newClient, msg);
        }
    }

    ret = 0;

cleanup:
    if (NULL != gPool)
    {
        THreadPoolDestroy(gPool);
        gPool = NULL;
    }

    CloseHandle(gMutexClients);
    CloseHandle(gMutexSend);

    if (NULL != gClients)
    {
        for (int i = 0; i < gNumberThreads; i++)
        {
            if (gClients[i] != NULL)
            {
                free(gClients[i]);
            }
        }
        free(gClients);
    }

    DestroyServer(gServer);
    UninitCommunicationModule();

    return ret;
}