#pragma once

#include <WinSock2.h>
#pragma comment(lib,"ws2_32.lib")
#define BUFFER_SIZE 4*1024

class SocketContext
{
public:
	SOCKET s;
	sockaddr_in addr;
	
	SocketContext() {
		ZeroMemory(this, sizeof(SocketContext));
	}
};

class IOContext
{
public:
	OVERLAPPED ol;
	DWORD Type;
	DWORD length;
	CHAR buffer[BUFFER_SIZE];

	IOContext(DWORD type) {
		ZeroMemory(this, sizeof(IOContext));
		Type = type;
	}
};

enum {
	IO_READ = 1,
	IO_WRITE,
};

class IOCPServer 
{
public:


	HANDLE m_hCompletionPort;
	SOCKET m_sListenSocket;

	typedef void (CALLBACK *pfnNotifyProc)(IOCPServer* iocp,SocketContext* pPerSocket, IOContext*   pPerIO);
	
	pfnNotifyProc m_NotifyProc;

	bool StartServer(pfnNotifyProc NotifyProc,USHORT uport);

	VOID PostRecv(SocketContext * pPerSocket);

	VOID PostSend(SocketContext * pPerSocket, BYTE * buffer, size_t len);



	static DWORD ListenThreadProc(LPVOID lParam);

	static DWORD WorkThreadProc(LPVOID lParam);


	IOCPServer();

	~IOCPServer();

};