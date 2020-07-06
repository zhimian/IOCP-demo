#include <stdio.h>
//#include <vld.h>
#include "IOCPServer.h"



bool IOCPServer::StartServer(pfnNotifyProc NotifyProc,USHORT uPort)
{	
	m_NotifyProc = NotifyProc;
	//1.创建完成端口
	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (m_hCompletionPort == NULL || m_hCompletionPort == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	//2.创建工作线程
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	int nProcessors = si.dwNumberOfProcessors;

	HANDLE hWorkThread;
	for (int i = 0; i < nProcessors*2; i++)
	{
		hWorkThread = (HANDLE)CreateThread(NULL,
			0,
			(LPTHREAD_START_ROUTINE)WorkThreadProc,
			(LPVOID)this,
			0,
			NULL);
		if (hWorkThread == NULL)
		{
			CloseHandle(m_hCompletionPort);
			return FALSE;
		}
		CloseHandle(hWorkThread);
	}

	//3.初始化监听socket
	m_sListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_sListenSocket == INVALID_SOCKET)
		return FALSE;
	
	SOCKADDR_IN	ServerAddr;
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = INADDR_ANY;
	ServerAddr.sin_port = htons(uPort);

	int iRet = bind(m_sListenSocket,
		(sockaddr*)&ServerAddr,
		sizeof(ServerAddr));
	if (iRet == SOCKET_ERROR)
	{
		int a = GetLastError();
		closesocket(m_sListenSocket);
		return FALSE;
	}

	iRet = listen(m_sListenSocket, SOMAXCONN);
	if (iRet == SOCKET_ERROR)
	{
		closesocket(m_sListenSocket);
		return FALSE;
	}

	//4.创建线程用于accept
	HANDLE hListenThread = (HANDLE)CreateThread(NULL,
		0,
		(LPTHREAD_START_ROUTINE)ListenThreadProc,
		(LPVOID)this,
		0,
		NULL);
	if (hListenThread == NULL)
	{
		CloseHandle(m_hCompletionPort);
		closesocket(m_sListenSocket);
		return FALSE;
	}
	CloseHandle(hListenThread);

	printf("[INFO] IOCP server started!\n");
}


//
DWORD IOCPServer::ListenThreadProc(LPVOID lParam) {
	
	IOCPServer* This = (IOCPServer*)lParam;

	while (TRUE) {
		//1.获取连接socket
		SOCKADDR_IN ClientAddr;
		int len = sizeof(SOCKADDR_IN);
		SOCKET sClient = accept(This->m_sListenSocket,
			(sockaddr*)&ClientAddr,
			&len);
		if (sClient == INVALID_SOCKET)
		{
			continue;
		}
		printf("[INFO] accepted connection!\n");


		//2.将完成端口与连入socket绑定
		SocketContext* pPerSocket = new SocketContext;
		pPerSocket->s = sClient;
		memcpy(&pPerSocket->addr, &sClient, len);
		HANDLE hcp = CreateIoCompletionPort((HANDLE)sClient,
			This->m_hCompletionPort,
			(ULONG_PTR)pPerSocket,
			0
		);
		if (hcp != This->m_hCompletionPort)
		{
			printf("[ERROR] bind CompletionPort error!\n");

			delete pPerSocket;
			CancelIo((HANDLE)sClient);
			closesocket(sClient);
			continue;
		}


		//3.收消息 投递recv请求
		//将iodata中的数据缓冲区作为recv的接收缓冲区
		This->PostRecv(pPerSocket);
	}
	
	return 0;
}

VOID IOCPServer::PostRecv(SocketContext* pPerSocket) 
{
	IOContext* pPerIO = new IOContext(IO_READ);

	WSABUF wsabuf;
	wsabuf.buf = pPerIO->buffer;
	wsabuf.len = BUFFER_SIZE;

	DWORD dwRecv = 0, dwFlag = 0;
	int ret = WSARecv(pPerSocket->s, &wsabuf, 1, &dwRecv, &dwFlag, &(pPerIO->ol), NULL);
	if (ret == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {

		printf("[ERROR] WSARecv error:%d event:%d!\n", WSAGetLastError(), pPerIO->ol.hEvent);
		CancelIo((HANDLE)pPerSocket->s);
		closesocket(pPerSocket->s);
		delete pPerSocket;
		delete pPerIO;
	}
}

VOID IOCPServer::PostSend(SocketContext *pPerSocket,BYTE* buffer,size_t len)
{
	size_t pos = 0;
	while (len) {
		IOContext* pPerIO = new IOContext(IO_WRITE);
		WSABUF wsabuf;
		wsabuf.buf = (char *)buffer+pos;
		
		if (len >= BUFFER_SIZE) {
			len -= BUFFER_SIZE;
			wsabuf.len = BUFFER_SIZE;
			pos += BUFFER_SIZE;
		}
		else
		{
			wsabuf.len = len;
			len = 0;
		}

		DWORD dwRecv = 0;
		DWORD dwFlag = MSG_PARTIAL;

		int iOk = WSASend(pPerSocket->s,
			&wsabuf, 1,
			&dwRecv,
			dwFlag,
			&pPerIO->ol,
			NULL);
		if (iOk == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING)
		{
			int a = GetLastError();

		}
	}

}


DWORD IOCPServer::WorkThreadProc(LPVOID lParam) {
	IOCPServer* This = (IOCPServer*)(lParam);
	HANDLE   hCompletionPort = This->m_hCompletionPort;
	SocketContext*  pPerSocket = NULL;
	IOContext*   pPerIO = NULL;
	DWORD dwTrans = 0;

	while (true) {
		BOOL bRet = GetQueuedCompletionStatus(
			hCompletionPort,
			&dwTrans,
			(LPDWORD)&pPerSocket,
			(LPOVERLAPPED*)&pPerIO, INFINITE);
		
		pPerIO->length = dwTrans;

		DWORD dwIOError = GetLastError();
		if (bRet == FALSE)
		{
			if (dwIOError != WAIT_TIMEOUT)
			{
				printf("[INFO] connection closed!\n");
			}
			closesocket(pPerSocket->s);
			delete pPerSocket;
			delete pPerIO;
			continue;
		}

		if (!dwTrans && pPerIO->Type) {
			printf("[INFO] connection closed!\n");

			closesocket(pPerSocket->s);
			delete pPerSocket;
			delete pPerIO;
			continue;
		}

		switch (pPerIO->Type)
		{
		case IO_READ:
		{
			This->m_NotifyProc(This,pPerSocket, pPerIO);
			delete pPerIO;

			This->PostRecv(pPerSocket);
			break;
		}
		case IO_WRITE:
		{
			printf("[INFO] send content:%.*s\n", dwTrans, pPerIO->buffer);
			printf("[INFO] send complete!\n");
			delete pPerIO;
			break;
		}
		default:
			break;
		}

	}

	return 0;
}

IOCPServer::IOCPServer()
{
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (err != 0)
	{
		printf("[ERROR] WSAStartup failed!");
		return;
	}

}

IOCPServer::~IOCPServer()
{

	WSACleanup();
}


