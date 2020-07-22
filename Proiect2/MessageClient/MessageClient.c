// HEADERClient.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"

#include "communication_api.h"

#include <windows.h>
#include <stdio.h>

//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

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
Structure to pack receiver thread parameter, just contains client so receiver can actually receive data from server
*/
typedef struct _THREADPARAMREC
{
    CM_CLIENT* Client;
}THREADPARAMREC;

/*
Structure to pack file sender thread parameter, contains client so the file can be sent and filename
*/
typedef struct _THREADPARAMSEND
{
    CM_CLIENT* Client;
    char* FileName;
}THREADPARAMSEND;

HANDLE gEventStartSending;     //evemt to signal file sender thread that it can start sending file
HANDLE gSenderThread;          //handle to sender thread, needs to be global in order to close it properly in receiver thread
HANDLE gMutexSend;             //mutex to sync data sending to server, so a file can be sent asynchronous

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
Validates given Buffer as input for register command client side
Returns 0 on success, -1 on error
*/
int ValidateUserRegister(unsigned char* Buffer)
{
    if (Buffer == NULL)
    {
        //invalid parameter 
        return -1;
    }
    char* temp = NULL;
    temp = (char*)malloc(strlen((const char*)Buffer) + 1); //temporary buffer, i will validate using this so original data won't be affected
    strcpy_s(temp, strlen((const char*)Buffer) + 1, (const char*)Buffer);

    char* iter = strtok((char*)temp, " ");
    if (iter == NULL)
    {
        free(temp);
        return -1;
    }
    else
    {
        //validate user now 
        for (int i = 0; iter[i]; i++)
        {
            if (!isalpha(iter[i]) && !isdigit(iter[i]))
            {
                //username does not follow restrictions
                _tprintf_s(TEXT("Error: Invalid username\n"));
                free(temp);
                return -1;
            }
        }
        iter = strtok(NULL, " ");
        if (iter == NULL)
        {
            free(temp);
            return -1;
        }
        //validate password now
        char* space = strtok(NULL, " ");
        if (space != NULL)
        {
            //password cannot contain space
            free(temp);
            _tprintf_s(TEXT("Error: Invalid password\n"));
            return -1;
        }

        if (strlen(iter) < 6) //6 because \n is in there as well
        {
            _tprintf_s(TEXT("Error: Password too weak\n"));
            free(temp);
            return -1;
        }
        
        int nonAlphaNumeric = 0; //used to signal the fact that a nonAlphaNumeric character is present
        int upperCase = 0; //used to signal the fact that an uppercase character is present
        int backslashpos = (int)strlen(iter);
        iter[backslashpos - 1] = '\0';
        for (int i = 0; iter[i]; i++)
        {
            if (iter[i] == ',')
            {
                //password cannot contain comma
                _tprintf_s(TEXT("Error: Invalid password\n"));
                free(temp);
                return -1;
            }
            if (isupper(iter[i]))
            {
                upperCase += 1;
            }
            if (!isalpha(iter[i]) && !isdigit(iter[i]))
            {
                //nonalphanumeric character
                nonAlphaNumeric += 1;
            }
        }
        iter[backslashpos - 1] = '\n';

        if (upperCase != 0 && nonAlphaNumeric != 0)
        {
            //good password
            free(temp);
            return 0;
        }
        else
        {
            _tprintf_s(TEXT("Error: Password too weak\n"));
            free(temp);
            return -1;
        }
    }
}

/*
Validates given Buffer as input for login command client side
Returns 0 on success, -1 on error
*/
int ValidateUserLogin(unsigned char* Buffer)
{
    if (Buffer == NULL)
    {
        //invalid parameter
        return -1;
    }

    int space = 0; //string must contain only 1 space in order to be valid
    for (int i = 0; Buffer[i]; i++)
    {
        if (Buffer[i] == ' ')
        {
            space += 1;
        }
    }
    
    if (space != 1)
    {
        //invalid string
        return -1;
    }
    else
    {
        return 0;
    }
}

/*
Validates given Buffer as input for msg command client side
Returns 0 on success, -1 on error
*/
int ValidateSendMessage(unsigned char* Buffer)
{
    if (Buffer == NULL)
    {
        //invalid param
        return -1;
    }

    for (int i = 0; Buffer[i]; i++)
    {
        if (Buffer[i] == ' ')
        {
            return 0;
        }
    }

    return -1;
}

/*
Validates given Buffer as input for history command client side
Returns 0 on success, -1 on error
*/
int ValidateHistory(unsigned char* Buffer)
{
    if (Buffer == NULL)
    {
        //invalid parameter
        return -1;
    }
    char* copyBuffer = (char*)malloc(strlen((char*)Buffer) + 1);
    if (copyBuffer == NULL)
    {
        //alloc error
        return -1;
    }
    strcpy_s(copyBuffer, strlen((char*)Buffer) + 1, (const char*)Buffer);

    int size = (int)strlen((const char*)copyBuffer);
    copyBuffer[size - 1] = '\0';

    char* user = strtok((char*)copyBuffer, " ");
    if (user == NULL)
    {
        //no space, something is wrong
        free(copyBuffer);
        return -1;
    }
    char* count = strtok(NULL, " ");
    if (count == NULL)
    {
        free(copyBuffer);
        return -1;
    }

    for (int i = 0; count[i]; i++)
    {
        if (!isdigit(count[i]))
        {
            free(copyBuffer);
            return -1;
        }
    }
    free(copyBuffer);
    return 0;
}

/*
Validates given Buffer as input for sendfile command client side
Returns 0 on success, -1 on error
*/
int ValidateSendFile(unsigned char* Buffer)
{
    if (Buffer == NULL)
    {
        //invalid parameter
        return -1;
    }
    char* copy = (char*)malloc(strlen((char*)Buffer) + 1);
    if (copy == NULL)
    {
        //alloc error
        _tprintf_s(TEXT("Unexpected error: Failed to alloc temp buffer when validating send file!\n"));
        return -1;
    }
    strcpy(copy, (const char*)Buffer);

    char* user = strtok(copy, " ");
    char* file = strtok(NULL, " ");

    (void)user;  //to clear warning

    int last = (int)strlen(file);
    file[last - 1] = '\0';   //replace \n with \0

    
    FILE* filePointer = NULL;
    if (fopen_s(&filePointer, file, "rb") != 0)
    {
        _tprintf_s(TEXT("Error: File not found\n"));
        free(copy);
        return -1;
    }

    fclose(filePointer);
    free(copy);
    return 0;
}

/*
Function that assigns validation if neccessary to right function for input Buffer and Header
Returns 0 on success, -1 on error
*/
int ValidateClientSide(CM_HEADER Header, unsigned char* Buffer)
{
    if (Buffer == NULL)
    {
        //invalid parameter
        return -1;
    }

    switch (Header.Cmd)
    {
    case 2:
        return ValidateUserRegister(Buffer);
    case 3:
        return ValidateUserLogin(Buffer);
    case 5:
        return ValidateSendMessage(Buffer);
    case 7:
        return ValidateSendFile(Buffer);
    case 10:
        return ValidateHistory(Buffer);
    default:
        return 0;
    }
}

/*
Sends input Header struct and Buffer to server according to protocol(first header containing buffer size and cmd) and after that the input Buffer
Returns 0 on success, -1 on error
*/
int SendDataToServerFromClient(CM_CLIENT* Client, unsigned char* Buffer, CM_HEADER* Header)
{
    if (Client == NULL || Buffer == NULL || Header == NULL)
    {
        //invalid parameters
        return -1;
    }

    CM_DATA_BUFFER* dataToSend = NULL;
    CM_ERROR error = CreateDataBuffer(&dataToSend, Max(Header->Size, (DWORD)sizeof(CM_HEADER)));   //buffer length will be of max from string size or struct header
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Failed to create SEND data buffer with err-code=0x%X!\n"), error);
        return -1;
    }

    //firstly i send the struct header
    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)Header, sizeof(CM_HEADER));
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    CM_SIZE sendBytesCount = 0;
    error = SendDataToServer(Client, dataToSend, &sendBytesCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }
    if (sendBytesCount != sizeof(CM_HEADER))
    {
        //not all data was sent
        DestroyDataBuffer(dataToSend);
        _tprintf_s(TEXT("Unexpected error: Not all bytes were sent to server(struct)!\n"));
        return -1;
    }

    error = CopyDataIntoBuffer(dataToSend, (const CM_BYTE*)Buffer, Header->Size);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: CopyDataIntoBuffer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }

    sendBytesCount = 0;
    error = SendDataToServer(Client, dataToSend, &sendBytesCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: SendDataToServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToSend);
        return -1;
    }
    if (sendBytesCount != Header->Size)
    {
        //not all data was sent
        DestroyDataBuffer(dataToSend);
        _tprintf_s(TEXT("Unexpected error: Not all bytes were sent to server(buffer)!\n"));
        return -1;
    }

    DestroyDataBuffer(dataToSend);
    return 0;
}

/*
Receives header from server into Header given parameter
Returns 0 on success, -1 on error
*/
int ReceiveHeader(CM_CLIENT* Client, CM_HEADER** Header)
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
    error = ReceiveDataFormServer(Client, dataToReceive, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: ReceiveDataFromServer failed with err-code=0x%X!\n"), error);
        DestroyDataBuffer(dataToReceive);
        return -1;
    }
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
Receives buffer from server into Buffer parameter. Header parameter used to validate that all bytes arrived at client
Returns 0 on success, -1 on error
*/
int ReceiveBuffer(CM_CLIENT* Client, CM_HEADER* Header, CM_DATA_BUFFER* Buffer)
{
    if (Client == NULL || Header == NULL || Buffer == NULL)
    {
        //invalid parameters
        return -1;
    }

    CM_SIZE receivedByteCount = 0;
    CM_ERROR error = ReceiveDataFormServer(Client, Buffer, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
        return -1;
    }

    if (receivedByteCount != Header->Size)
    {
        return -1;
    }
    return 0;
}

/*
Returns corresponding number from given command inside String parameter. Number returned will be used inside a CM_HEADER structure
Returns 64 if error or command is not valid
*/
DWORD GiveNumberFromCommand(unsigned char* String)
{
    if (String == NULL)
    {
        //invalid parameter
        return 64;
    }

    int retVal = -1;
    retVal = strcmp((const char*)String, "echo");
    if (retVal == 0)
    {
        return 1;
    }

    retVal = strcmp((const char*)String, "register");
    if (retVal == 0)
    {
        return 2;
    }

    retVal = strcmp((const char*)String, "login");
    if (retVal == 0)
    {
        return 3;
    }

    retVal = strcmp((const char*)String, "logout\n");
    if (retVal == 0)
    {
        return 4;
    }

    retVal = strcmp((const char*)String, "msg");
    if (retVal == 0)
    {
        return 5;
    }

    retVal = strcmp((const char*)String, "broadcast");
    if (retVal == 0)
    {
        return 6;
    }

    retVal = strcmp((const char*)String, "sendfile");
    if (retVal == 0)
    {
        return 7;
    }

    retVal = strcmp((const char*)String, "list\n");
    if (retVal == 0)
    {
        return 8;
    }

    retVal = strcmp((const char*)String, "exit\n");
    if (retVal == 0)
    {
        return 9;
    }

    retVal = strcmp((const char*)String, "history");
    if (retVal == 0)
    {
        return 10;
    }

    //invalid command case
    return 64;
}

/*
This function will receive data from server. The function should be called from separate thread
Returns 0 on success, 1 if error
*/
DWORD WINAPI ReceiveFunc(PVOID Parameters)
{
    if (Parameters == NULL)
    {
        //invalid parameters
        return 1;
    }

    THREADPARAMREC* givenParam = (THREADPARAMREC*)Parameters; //unpacking parameter

    CM_HEADER* headerToRec = (CM_HEADER*)malloc(sizeof(CM_HEADER));
    if (headerToRec == NULL)
    {
        //alloc error
        _tprintf_s(TEXT("Unexpected error: Failed to alloc receive header!\n"));
        UninitCommunicationModule();
        return 1;
    }

    char* fileName = NULL;  //used when receiving file
    FILE* filePointer = NULL; //used when receiving file

    while (1)
    { 
        int retVal = ReceiveHeader(givenParam->Client, &headerToRec);
        if (retVal != 0)
        {
            UninitCommunicationModule();
            _tprintf_s(TEXT("Unexpected error: Failed receiving data from server!\n"));
        }

        CM_DATA_BUFFER* dataToReceive = NULL;
        CM_ERROR error = CreateDataBuffer(&dataToReceive, headerToRec->Size);
        if (CM_IS_ERROR(error))
        {
            _tprintf_s(TEXT("Unexpected error: Failed to create RECEIVE data buffer with err-code=0x%X!\n"), error);
            UninitCommunicationModule();
        }

        retVal = ReceiveBuffer(givenParam->Client, headerToRec, dataToReceive);
        if (retVal != 0)
        {
            DestroyDataBuffer(dataToReceive);
            UninitCommunicationModule();
        }

        if (headerToRec->Cmd == 55)
        {
            //received filename and prepare to receive a file
            fileName = (char*)malloc((strlen((const char*)dataToReceive->DataBuffer) + 1) * sizeof(char));
            if (fileName == NULL)
            {
                //alloc error
                _tprintf_s(TEXT("Unexpected error: Failed to alloc fileName!\n"));
            }
            int size = (int)strlen((const char*)dataToReceive->DataBuffer);
            dataToReceive->DataBuffer[size - 1] = '\0'; //clear \n

            strcpy(fileName, (const char*)dataToReceive->DataBuffer);

            if (fopen_s(&filePointer, fileName, "wb") != 0)
            {
                //error opening file
                _tprintf_s(TEXT("Unexpected error: Failed to open file to append!\n"));
            }
        }
        else if (headerToRec->Cmd == 88)
        {
            //part of file, just write to already opened file
            fwrite(dataToReceive->DataBuffer, sizeof(unsigned char), headerToRec->Size, filePointer);
        }
        else if (headerToRec->Cmd == 75)
        {
            //file was received, just close file and wrap up
            fclose(filePointer);
            char* successMsg = NULL;
            successMsg = (char*)malloc((strlen("Received file:  from  ") + strlen(fileName) + strlen((const char*)dataToReceive->DataBuffer) + 1) * sizeof(char)); //alloc space for succes message
            if (successMsg == NULL)
            {
                //alloc errror
                _tprintf_s(TEXT("Unexpected error: Failed to alloc succes mess!\n"));
            }

            //build success message
            strcpy(successMsg, "Received file: ");
            strcat(successMsg, fileName);
            strcat(successMsg, " from ");
            strcat(successMsg, (const char*)dataToReceive->DataBuffer);
            strcat(successMsg, "\n");

            _tprintf_s(TEXT("%hs"), successMsg);

            free(fileName);
            free(successMsg);
            fileName = NULL;
        }
        else if (headerToRec->Cmd == 9)
        {
            //exit code received from server, shut down thread
            free(headerToRec);
            DestroyDataBuffer(dataToReceive);
            return 0;
        }
        else if (headerToRec->Cmd == 7)
        {
            //either i received ok to send file, so i signal that by setting event
            //either i received succes sending
            //either i received error
            int ok = strcmp((const char*)dataToReceive->DataBuffer, "Success\n");
            if (ok == 0)
            {
                //can start sending file
                SetEvent(gEventStartSending);
                _tprintf_s(TEXT("%hs"), dataToReceive->DataBuffer);
            }
            else
            {
                //i have error, print it and close handle of sender thread
                WaitForSingleObject(gSenderThread, INFINITE);
                CloseHandle(gSenderThread);
                _tprintf_s(TEXT("%hs"), dataToReceive->DataBuffer);
            }
        }
        else
        {
            //just print what was received
            _tprintf_s(TEXT("%hs"), dataToReceive->DataBuffer);
        }
        
        DestroyDataBuffer(dataToReceive);
    }  
}

/*
This function will send a file to the server to be later send to another client. Function should be called from separate thread.
Function starts only if gEventStartSending is signaled, which is done when OK is received from server to start sending
gMutexSend makes sure that data is sent protected so if user sends another message to server while file is sending the data will be processed correctly
Returns 0 on success, 1 if error
*/
DWORD WINAPI SendFileFunc(PVOID Parameters)
{
    if (Parameters == NULL)
    {
        //invalid parameter
        return 1;
    }

    THREADPARAMSEND* param = (THREADPARAMSEND*)Parameters;

    DWORD result = WaitForSingleObject(gEventStartSending, 1000);

    if (result == WAIT_OBJECT_0)
    {
        //go on, received ok from server
        unsigned char* buffer = (unsigned char*)malloc(4096 * sizeof(unsigned char)); //4kb buffer to send file
        if (buffer == NULL)
        {
            //alloc error
            _tprintf_s(TEXT("Unexpected error: Failed to create buffer to send file!\n"));
            ResetEvent(gEventStartSending);
            return 1;
        }

        FILE* filePointer = NULL;

        int size = (int)strlen(param->FileName);

        param->FileName[size - 1] = '\0';   //clear \n from filename

        if (fopen_s(&filePointer, param->FileName, "rb") != 0)
        {
            //failed to open for read
            _tprintf_s(TEXT("Unexpected error: Failed to open file to send!\n"));
            ResetEvent(gEventStartSending);
            free(buffer);
            return 1;
        }

        int bytesRead = 0;
        bytesRead = (int)fread(buffer, sizeof(unsigned char), 4096, filePointer);

        while (bytesRead != 0)
        {
            //send file to server 4kb at a time
            CM_HEADER* header = (CM_HEADER*)malloc(sizeof(CM_HEADER));
            if (header == NULL)
            {
                //alloc error
                _tprintf_s(TEXT("Unexpected error: Failed to alloc header!\n"));
                ResetEvent(gEventStartSending);
                free(buffer);
                fclose(filePointer);
                return 1;
            }

            header->Cmd = 88; //specific cmd so server knows part of file is coming
            header->Size = bytesRead;

            WaitForSingleObject(gMutexSend, INFINITE);
            int retVal = SendDataToServerFromClient(param->Client, buffer, header);
            ReleaseMutex(gMutexSend);
            
            if (retVal != 0)
            {
                _tprintf_s(TEXT("Unexpected error: Failed to send data!\n"));
                ResetEvent(gEventStartSending);
                free(buffer);
                free(header);
                fclose(filePointer);
                return 1;
            }

            free(header);
            bytesRead = (int)fread(buffer, sizeof(unsigned char), 4096, filePointer);
        }

        free(buffer);
        fclose(filePointer);

        CM_HEADER* header = (CM_HEADER*)malloc(sizeof(CM_HEADER));
        if (header == NULL)
        {
            //alloc error
            _tprintf_s(TEXT("Unexpected error: Failed to alloc header!\n"));
            ResetEvent(gEventStartSending);
            return 1;
        }

        //notify server that file finished sending and send filename
        header->Cmd = 75;
        header->Size = (int)strlen(param->FileName);

        WaitForSingleObject(gMutexSend, INFINITE);
        int retVal = SendDataToServerFromClient(param->Client, (unsigned char*)param->FileName, header);
        ReleaseMutex(gMutexSend);

        if (retVal != 0)
        {
            _tprintf_s(TEXT("Unexpected error: Failed to send succes msg after sending file!\n"));
            ResetEvent(gEventStartSending);
            free(header);
            return 1;
        }

        free(header);
        ResetEvent(gEventStartSending);

        char* successMsg = NULL;
        CHAR fullPath[255];
      
        GetFullPathNameA(param->FileName, MAX_PATH, fullPath, NULL);   //get full path of file that was sent

        successMsg = (char*)malloc((strlen("Succesfully sent file: ") + strlen(fullPath) + 1) * sizeof(char));
        if (successMsg == NULL)
        {
            //alloc error
            _tprintf_s(TEXT("Unexpected error: Failed to create success message for sender!\n"));
            return 1;
        }

        strcpy(successMsg, "Succesfully sent file: ");
        strcat(successMsg, fullPath);

        _tprintf_s(TEXT("%hs\n"), successMsg);

        free(successMsg);
        return 0;
    }
    else
    {
        //error at server, do nothing
        return 0;
    }
}

/*
Main client function
*/
int _tmain(int argc, TCHAR* argv[])
{
    int ret = -1; //final return value

    (void)argc; //to kill warning
    (void)argv; //to kill warning
    
    CM_SIZE dataToSendSize = 0;

    //EnableCommunicationModuleLogger();

    CM_ERROR error = InitCommunicationModule();
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: InitCommunicationModule failed with err-code=0x%X!\n"), error);
        return -1;
    }

    CM_CLIENT* client = NULL;
    error = CreateClientConnectionToServer(&client);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Error: no running server found\n"));
        goto cleanup;
    }
    
    CM_DATA_BUFFER* buffer = NULL;
    error = CreateDataBuffer(&buffer, (CM_SIZE)strlen(("Error: maximum concurrent connection count reached\n")) + 1);
    if (IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: Error creating data buffer\n"));
        goto cleanup;
    }

    //i wait to receive response from server, if i can connect or not, depending if there is room
    CM_SIZE receivedByteCount = 0;
    error = ReceiveDataFormServer(client, buffer, &receivedByteCount);
    if (CM_IS_ERROR(error))
    {
        _tprintf_s(TEXT("Unexpected error: ReceiveDataFromClient failed with err-code=0x%X!\n"), error);
        goto cleanup;
    }

    unsigned char* msg = (unsigned char*)buffer->DataBuffer;  //response from server

    //now check if i got ok from server if i can connect
    int retVal = -1;
    retVal = strcmp((char*)msg, "OK!");
    if (retVal != 0)
    {
        //no more room on server
        _tprintf_s(TEXT("%hs"), msg);
        DestroyDataBuffer(buffer);
        goto cleanup;
    }

    _tprintf_s(TEXT("Successful connection\n"));
    DestroyDataBuffer(buffer);

    unsigned char localBuffer[270]; //local buffer, will store the line commands given to client, acording to documentation, command lines should be of maximum 255 characters
    retVal = -1;

    //prepare handle and parameter for receiver thread
    HANDLE receiveThread;
    THREADPARAMREC* param = (THREADPARAMREC*)malloc(sizeof(THREADPARAMREC));
    if (param == NULL)
    {
        //error alloc
        _tprintf_s(TEXT("Unexpected error: Receive thread parameter could not be allocated!\n"));
        goto cleanup;

    }

    THREADPARAMSEND* paramSend = NULL;  //parameter for file sender thread

    gEventStartSending = CreateEvent(NULL, FALSE, FALSE, NULL);  //event to signal that file sender thread can start sending

    param->Client = client;

    //call ReceiveFunc in separate thread
    receiveThread = CreateThread(
        NULL,         
        0,            
        ReceiveFunc,   
        (PVOID)param,  
        0,            
        NULL         
    );  

    gMutexSend = CreateMutex(NULL, FALSE, NULL); //prepare mutex to sync data send to server

    while (TRUE)
    {
        if (!fgets((char*)localBuffer, 255, stdin))
        {
            _tprintf_s(TEXT("Unexpected error: Error reading string from console!\n"));
            goto cleanup;
        }
        dataToSendSize = (CM_SIZE)strlen((const char*)localBuffer);

        CM_HEADER header;

        //just clear space after command
        int tempToStop = -1;
        for (int i = 0; localBuffer[i]; i++)
        {
            if (localBuffer[i] == ' ')
            {
                tempToStop = i;
                localBuffer[i] = '\0';
                break;
            }
        }

        //get corresponding number from command
        DWORD result = GiveNumberFromCommand(localBuffer);

        //if command contained multiple words, put space back
        if (tempToStop != -1)
        {
            localBuffer[tempToStop] = ' ';
        }

        //invalid command or bad parameter
        if (result == 64)
        {
            //invalid command, nothing should be sent
            _tprintf_s(TEXT("Unexpected error: Invalid command!\n"));
        }
        else
        {
            header.Cmd = result;    
            header.Size = (DWORD)strlen((const char*)(localBuffer + tempToStop + 1));
            header.Size++; //place for null terminator, hence the increment
            retVal = ValidateClientSide(header, localBuffer + tempToStop + 1);    //validate data client side

            //if data is validated, continue
            if (retVal == 0)
            {
                //send data to server synced, wait for mutex first
                WaitForSingleObject(gMutexSend, INFINITE);
                retVal = SendDataToServerFromClient(client, localBuffer + tempToStop + 1, &header);
                ReleaseMutex(gMutexSend);

                if (retVal != 0)
                {
                    _tprintf_s(TEXT("Unexpected error: Failed sending data to server!\n"));
                    goto cleanup;
                }

                if (result == 7)
                {
                    //send file case, prepare file sender thread parameter and start function in separate thread
                    paramSend = (THREADPARAMSEND*)malloc(sizeof(THREADPARAMSEND));
                    if (paramSend == NULL)
                    {
                        //alloc error
                        _tprintf_s(TEXT("Unexpected error: Failed to alloc thread param sender!\n"));
                        goto cleanup;
                    }
                    else
                    {
                        unsigned char* buff = localBuffer + tempToStop + 1;
                        char* userName = strtok((char*)buff, " ");
                        char* fileName = strtok(NULL, " ");
                        (void)userName;     //i do not need userName, typecast to void to avoid warning
                        
                        //prepare parameter
                        paramSend->Client = client;
                        paramSend->FileName = fileName;

                        //call SendFileFunc in separate thread, thread will be waiting for event in order to start sending, depending of response from server
                        gSenderThread = CreateThread(
                            NULL,         
                            0,            
                            SendFileFunc,  
                            (PVOID)paramSend,  
                            0,            
                            NULL         
                        );
                    }
                }
                else if (result == 9)
                {
                    //exit case, just close receiver thread and free parameters
                    WaitForSingleObject(receiveThread, INFINITE);
                    CloseHandle(receiveThread);

                    //if file sender thread parameter was not freed, free it
                    if (paramSend != NULL)
                    {
                        free(paramSend);
                    }

                    //free receiver thread parameter
                    free(param);
                    break;
                }
            }
            else if(result != 2 && result != 7)  //case 2 and case 7 are printing error messages inside their validation functions, hence the if
            {
                _tprintf_s(TEXT("Unexpected error: Invalid data input!\n"));
            }
        }
    }

    ret = 0;  //if we got here, no error was encountered, so return code should be 0 
cleanup:
    if (client != NULL)
    {
        DestroyClient(client);
    }

    CloseHandle(gMutexSend);
    CloseHandle(gEventStartSending);

    UninitCommunicationModule();
    //_CrtDumpMemoryLeaks();

    return ret;
}

