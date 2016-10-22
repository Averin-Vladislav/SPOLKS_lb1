#include <iostream>
#include <fstream>
#include <string.h>
#include <string>
#pragma comment (lib, "ws2_32.lib")

#include <windows.h>
#include <conio.h>
#include <io.h>

using namespace std;

#define PORT_NUM 2222
#define SERVER_ADDRESS "192.168.30.1"
#define ECHO_CODE 1
#define TIME_CODE 2
#define CLOSE_CODE 3
#define UPLOAD_CODE 4
#define DOWNLOAD_CODE 5
#define ACK_CODE 6

#define BEGIN_PACKAGE_CODE 17
#define BEGIN_DATA_CODE 18
#define END_PACKAGE_CODE 19
#define END_OF_FILE_CODE 21

#define BLOCK_SIZE 1024

char webSocketDetails[1024];
SOCKET clientSocket;
string fileName;
long fileSize;

int sendMessage(string message);
char getCommandId(char* commandName);
bool handleResponse(string response);
int sendHeading(const char* heading);
long getFileSize(string fileName);
int sendFile(long startBlock);

int main() {
	setlocale(LC_ALL, "c");

	if (WSAStartup(0x202, (WSADATA *) webSocketDetails)) {
		printf("wsa startup error %d\n", WSAGetLastError());
		return -1;
	}

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket < 0) {
		printf("socket init error %d\n", WSAGetLastError());
		return -1;
	}

	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT_NUM);
	
	HOSTENT* host;

	if (inet_addr(SERVER_ADDRESS) != INADDR_NONE) {
		serverAddress.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDRESS);
	} else {
		if (host = gethostbyname(SERVER_ADDRESS)) {
			((unsigned long*)&serverAddress.sin_addr)[0] = ((unsigned long**)host->h_addr_list)[0][0];
		} else {
			printf("invalid address %s\n", SERVER_ADDRESS);
			closesocket(clientSocket);
			WSACleanup();
			return -1;
		}
	}

	if (connect(clientSocket, (sockaddr *) &serverAddress, sizeof(serverAddress))) {
		printf("connection error %d\n", WSAGetLastError());
		return -1;
	}

	printf("connection was established\n");

	int nSize;
	string message;
	string response = "";
	bool nextCommand = true;

	while (true) {
		char buffer[BLOCK_SIZE] = { '\0' };
		nSize = recv(clientSocket, &buffer[0], sizeof(buffer) - 1, 0);
		for (int i = 0; i < sizeof(buffer); i++) {
			if (buffer[i] == '\0') {
				break;
			}

			response += buffer[i];

			if (buffer[i] == '\n' || buffer[i] == CLOSE_CODE) {
				buffer[0] = '\0';
				nextCommand = handleResponse(response);
				response = "";
			}
		
		}
		if (response.compare("") || !nextCommand) {
			continue;
		}
		do {
			printf("client -> server: ");
			getline(cin, message);
			message += '\n';
		} while (sendMessage(message) == -1);
	}

	printf("reading error %d\n", WSAGetLastError());
	closesocket(clientSocket);
	WSACleanup();
	return -1;
}

int sendMessage(string message) {
	int blocksCount = (message.size() / BLOCK_SIZE) + 1;
	int blockBegin = 0;
	string block;

	for (int i = 0; i < blocksCount - 1; i++) {
		block = message.substr(blockBegin, BLOCK_SIZE);

		if (blockBegin == 0) {
			if (sendHeading(block.c_str()) == SOCKET_ERROR) {
				return -1;
			} 
			blockBegin += BLOCK_SIZE;
			continue;
		} 

		if (send(clientSocket, block.c_str(), BLOCK_SIZE, 0) == SOCKET_ERROR) {
			return -1;
		}

		blockBegin += BLOCK_SIZE;
	}
	
	block = message.substr(blockBegin, message.size() - blockBegin);

	if (blockBegin == 0) {
		if (sendHeading(block.c_str()) == SOCKET_ERROR) {
			return -1;
		}
		return 0;
	}

	if (send(clientSocket, block.c_str(), BLOCK_SIZE, 0) == SOCKET_ERROR) {
		return -1;
	}

	return 0;
}

int sendHeading(const char* heading) {
	char commandName[10] = {'\0'};
	char parameter[BLOCK_SIZE] = {'\0'};
	char commandCode;
	string request = "";
	int i = -1;
 	bool hasParameter = true;

	while (heading[++i] != ' ' && heading[i] != '\n');
	memcpy(&commandName, heading, i);
	if (heading[i] == '\n') {
		hasParameter = false;
	} else {
		while (heading[++i] == ' ');
		memcpy(&parameter, &heading[i], strlen(heading) - i - 1);
	}

	if ((commandCode = getCommandId(commandName)) == 0) {
		printf("unperforming command\n");
		return -1;
	}

	request += commandCode;

	if (commandCode == UPLOAD_CODE) {
		fileName = parameter;
		fileSize = getFileSize(fileName);
		request += ' ' + to_string(fileSize) + ' ';
	}

	if (hasParameter) {
		request += parameter;
		request += '\n';
	}
	return send(clientSocket, request.c_str(), request.size(), 0);	
}

long getFileSize(string fileName) {
	struct stat stat_buf;
	int rc = stat(fileName.c_str(), &stat_buf);
	return rc == 0 ? stat_buf.st_size : -1;
}

char getCommandId(char* commandName) {
	if (!strcmp(commandName, "ECHO")) {
		return ECHO_CODE;
	} 
	if (!strcmp(commandName, "TIME")) {
		return TIME_CODE;
	}
	if (!strcmp(commandName, "CLOSE")) {
		return CLOSE_CODE;
	}
	if (!strcmp(commandName, "UPLOAD")) {
		return UPLOAD_CODE;
	}
	if (!strcmp(commandName, "DOWNLOAD")) {
		return DOWNLOAD_CODE;
	}

	return 0;
}

bool handleResponse(string response) {
	int commandCode = response.at(0);
	if (commandCode == TIME_CODE || commandCode == ECHO_CODE) {
		cout << "server -> client: " << response.substr(1, response.size() - 2).c_str() << endl;
		return true;
	}
	if (commandCode == CLOSE_CODE) {
		closesocket(clientSocket);
		WSACleanup();
		TerminateProcess(GetCurrentProcess(), 0);
	}
	if (commandCode == ACK_CODE) {
		if (response.at(1) == '\n' ||
			(sendFile(stol(response.substr(1, response.size() - 2))) == SOCKET_ERROR)) {
				return true;
		}
		return false;
	}
}

ifstream file;
long blockNum;

#define SPECIAL_GAVNO 10
#define PORTION_SIZE 20

int sendFile(long startBlock) {
	if (!file.is_open()) {
		file.open(fileName, ios::binary | ios::in);
		blockNum = fileSize / (BLOCK_SIZE - SPECIAL_GAVNO) + 1;
	}

	file.seekg(startBlock * (BLOCK_SIZE - SPECIAL_GAVNO), ios::beg);

	long currentBlock = startBlock;

	for (int i = 0; i < PORTION_SIZE && currentBlock <= blockNum - 1; i++) {
		/*string portion = "";
		char block[BLOCK_SIZE - SPECIAL_GAVNO] = { '\0' };
		portion += BEGIN_PACKAGE_CODE;
		string currentBlockString = to_string(currentBlock);
		while (currentBlockString.size() != SPECIAL_GAVNO - 2) {
			currentBlockString = '0' + currentBlockString;
		}
		portion += currentBlockString;
		portion += BEGIN_DATA_CODE;
		file.read(block, BLOCK_SIZE - SPECIAL_GAVNO);
		portion += block;
		portion += END_PACKAGE_CODE;
		if (currentBlock == blockNum - 1) {
			portion += END_OF_FILE_CODE;
		}
		send(clientSocket, portion.c_str(), portion.size(), 0);*/


		char portion[BLOCK_SIZE] = { '\0' };
		string currentBlockString = to_string(currentBlock);
		while (currentBlockString.size() != SPECIAL_GAVNO - 3) {
			currentBlockString = '0' + currentBlockString;
		}
		portion[0] = BEGIN_PACKAGE_CODE;
		memcpy(&portion[1], currentBlockString.c_str(), currentBlockString.size());
		portion[8] = BEGIN_DATA_CODE;
		long size = fileSize - file.cur > BLOCK_SIZE - SPECIAL_GAVNO ? BLOCK_SIZE - SPECIAL_GAVNO : fileSize - file.cur;
		file.read(&portion[9], size);
		portion[9 + size] = currentBlock == blockNum - 1 ? END_OF_FILE_CODE : END_PACKAGE_CODE;
		send(clientSocket, portion, BLOCK_SIZE, 0);
		cout << currentBlock << endl;
		currentBlock++;
	}
	return 0;
}

