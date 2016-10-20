#include <iostream>
#include <string.h>
#include <string>
#pragma comment (lib, "ws2_32.lib")

#include <windows.h>
#include <conio.h>

using namespace std;

#define PORT_NUM 2222
#define QUEUE_LENGTH 20
#define HELLO_STRING "Hi, this is server!\n"
#define ECHO_CODE 1
#define TIME_CODE 2
#define CLOSE_CODE 3
#define UPLOAD_CODE 4
#define DOWNLOAD_CODE 5

char* cbuf = "";
DWORD WINAPI workWithClient(LPVOID clientSocket);
DWORD WINAPI checkCommandBuffer(LPVOID clientSocket);

int clientAmount = 0;

void proccessCommand(string command);

int main() {
	char buf[1024];

	if (WSAStartup(0x0202, (WSADATA *) &buf[0])) {
		printf("wsa startup error %d\n", WSAGetLastError());
		return -1;
	}

	SOCKET serverSocket;
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socket init error %d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}

	sockaddr_in localAddress;
	localAddress.sin_family = AF_INET;
	localAddress.sin_port = htons(PORT_NUM);
	localAddress.sin_addr.S_un.S_addr = 0;

	if (bind(serverSocket, (sockaddr *) &localAddress, sizeof(localAddress))) {
		printf("bind socket to localAddress error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	if (listen(serverSocket, QUEUE_LENGTH)) {
		printf("listen error %d\n", WSAGetLastError());
		closesocket(serverSocket);
		WSACleanup();
		return -1;
	}

	printf("waiting for connection...\n");

	SOCKET clientSocket;
	sockaddr_in clientAddress;
	int clientAddressSize = sizeof(clientAddress);

	while (clientSocket = accept(serverSocket, 
								(sockaddr *) &clientAddress, 
								&clientAddressSize)) {
		if (clientAmount == 1) {
			continue;
		}

		clientAmount++;

		HOSTENT* host;
		host = gethostbyaddr((char *) &clientAddress.sin_addr.S_un.S_addr, 4, AF_INET);
		printf("+%s [%s] new connect\n", (host) ? host->h_name : "", inet_ntoa(clientAddress.sin_addr));

		workWithClient(&clientSocket);
	}

	return 0;
}

DWORD WINAPI workWithClient(LPVOID clientSocket) {
	SOCKET newSocket;
	newSocket = ((SOCKET *)clientSocket)[0];
	char buffer[1024] = "";
	send(newSocket, HELLO_STRING, sizeof(HELLO_STRING), 0);

	int receivedBytes;
	string command = "";

	while (true) {
		receivedBytes = recv(newSocket, buffer, sizeof(buffer), 0);

		for (int i = 0; i < sizeof(buffer); i++) {
			if (buffer[i] == '\0') {
				break;
			}

			command += buffer[i];
			
			if (buffer[i] == '\n') {
				proccessCommand(command);
			}
		}
	}

	clientAmount--;
	printf("disconnect client...\n");
	closesocket(newSocket);

	return 0;
}

void proccessCommand(string command) {
	string foo = command.substr(1, command.size() - 1);

	switch (command.at(0)) {
		case ECHO_CODE: cout << "ECHO command with " << foo << endl;
			break;
		case TIME_CODE: printf("TIME command\n");
			break;
		case CLOSE_CODE: printf("CLOSE command\n");
			break;
		case UPLOAD_CODE: cout << "UPLOAD command with " << foo << endl;
			break;
		case DOWNLOAD_CODE: cout << "DOWNLOAD command with " << foo << endl;
			break;
	}
}