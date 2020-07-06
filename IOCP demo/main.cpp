#include<stdio.h>
#include "IOCPServer.h"
char str[] = "Hello Client!";
void CALLBACK ReadHandle(IOCPServer* This,SocketContext *pPerSocket, IOContext*   pPerIO) {
	printf("[READ] length:%d\n", pPerIO->length);
	printf("[READ] content:%.*s\n", pPerIO->length, pPerIO->buffer);
	//Sleep(3000);
	
	//This->PostSend(pPerSocket, (BYTE *)str, strlen(str) + 1);
}


int main() {

	IOCPServer* pserver = new IOCPServer;
	bool bret = pserver->StartServer(ReadHandle,5678);
	if (!bret)
	{
		printf("start server error!");
	}
	getchar();

	return 0;
}