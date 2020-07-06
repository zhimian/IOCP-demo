
#include<stdio.h>
#include<winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main() {

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		printf("Failed to load Winsock");
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, 0);


	SOCKADDR_IN	ServerAddr;
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	ServerAddr.sin_port = htons(5678);

	int ret = connect(s, (struct  sockaddr*)&ServerAddr, sizeof(SOCKADDR_IN));
	if (ret == INVALID_SOCKET)
	{
		printf("connect error!");
	}
	printf("connected!\n");

	char str[] = "Hello Server!";
	char buffer[100];
	while (TRUE)
	{
		ret = send(s, str, strlen(str) + 1, 0);
		if (ret == SOCKET_ERROR)
		{
			printf("send error! code:%d \n", WSAGetLastError());
		}
		printf("send complete\n");
		recv(s, buffer, 100, 0);
		printf("receive:%s\n",buffer);
		getchar();
		closesocket(s);
		break;
	}

}