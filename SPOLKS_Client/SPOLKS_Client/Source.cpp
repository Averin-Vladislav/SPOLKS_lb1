#include "Common.h"

int init();
int proccessConnection();
int sendRequest(string request);
int getCommandCode(string message);
string getCommandPart(int commandCode, string request);
string getParameterPart(int commandCode, string request);
unsigned long long getParameterSize(string request);
unsigned long long getEchoSize(string request);
string getParameter(string request);
long getFileSize(string fileName);
string getEchoCommandPart(string request);
string getTimeCommandPart();
string getCloseCommandPart();
string getUploadCommandPart(string request);
string getDownloadCommandPart();
int readHello();
int processResponse();
void makeRequest();
int handleResponse(string response, u_long commandCode);
int getResponseCommandPartLength(u_long commandCode);
u_long getResponseCommandCode(char commandPart);
void writeMessageFromServer(string message);
int handleTimeResponse(string response);
int handleCloseResponse();
int handleUploadResponse(string response);
void disconnect();
string getStringFromPackageNum(u_long packageNum);
string getStringFromPackageSize(size_t packageSize);
string getStringFromBitset16(bitset<16> stringBitset);
string getStringFromBitset24(bitset<24> stringBitset);
bitset<24> getBitset24FromString(string string);
long getDataBlockSize(char* fileBlock);
bool reconnect();
bitset<40> getBitset40FromString(string string);
void writePackageToFile(string package, u_long packageSize);
int requestPackage(int currentPackageNum);
int handleDownloadResponse(string response);
int getPackageSizeFromString(string packageSizeString);
u_long getPackageNumFromString(string packageNumString);
int getFile(unsigned long long size);
bitset<16> getBitset16FromString(string string);
int checkPackageNum(string package, u_long currentPackageNum);


int main() {
	init();
}

int init() {
	setlocale(LC_ALL, "c");

	if (WSAStartup(0x202, (WSADATA *)webSocketDetails)) {
		printf("wsa startup error %d\n", WSAGetLastError());
		return -1;
	}

	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket < 0) {
		printf("socket init error %d\n", WSAGetLastError());
		return -1;
	}

	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(PORT_NUM);

	HOSTENT* host;

	if (inet_addr(SERVER_ADDRESS) != INADDR_NONE) {
		serverAddress.sin_addr.S_un.S_addr = inet_addr(SERVER_ADDRESS);
	}
	else {
		if (host = gethostbyname(SERVER_ADDRESS)) {
			((unsigned long*)&serverAddress.sin_addr)[0] = ((unsigned long**)host->h_addr_list)[0][0];
		}
		else {
			printf("invalid address %s\n", SERVER_ADDRESS);
			closesocket(clientSocket);
			WSACleanup();
			return -1;
		}
	}

	if (connect(clientSocket, (sockaddr *)&serverAddress, sizeof(serverAddress))) {
		printf("connection error %d\n", WSAGetLastError());
		return -1;
	}

	printf("connection was established\n");

	if (proccessConnection() == -1) {
		printf("reading error %d\n", WSAGetLastError());
		disconnect();
		return -1;
	}

	return 0;
}

int proccessConnection() {
	processing = false;

	if (readHello() == -1)
		return -1;

	while (true) {
		if (!processing) 
			makeRequest();
		if (processResponse() == -1) {
			return -1;
		}
	}
	disconnect();
	return -1;
}

void disconnect() {
	closesocket(clientSocket);
	WSACleanup();
}

void makeRequest() {
	string request;
	do {
		printf("client -> server: ");
		getline(cin, request);
	} while (sendRequest(request) == -1);
}

int processResponse() {
	char buffer[MAX_BLOCK_SIZE] = { '\0' };
	int bufSize;
	bool isCommandCodeProcessed = false;
	int commandPartLength = 1;
	u_long commandCode;
	string response = "";
	bufSize = recv(clientSocket, buffer, MAX_BLOCK_SIZE, 0);
	for (int i = 0; i < bufSize; i++) {
		response.push_back(buffer[i]);
		if (!isCommandCodeProcessed) {
			if ((commandCode = getResponseCommandCode(response.at(0))) == -1)
				return -1;

			commandPartLength = getResponseCommandPartLength(commandCode);
			isCommandCodeProcessed = true;
		}
		if (response.length() == commandPartLength) {
			if (handleResponse(response, commandCode) == -1)
				return -1;
			response.clear();
			isCommandCodeProcessed = false;
		}
	} 
	return 0;
}

int handleResponse(string response, u_long commandCode) {
	switch (commandCode) {
	case TIME_CODE:
		if (handleTimeResponse(response) == -1) {
			return -1;
		}
		break;
	case CLOSE_CODE:
		if (handleCloseResponse() == -1)
			return -1;
		break;
	case UPLOAD_CODE:
		if (handleUploadResponse(response) == -1)
			return -1;
		break;
	case DOWNLOAD_CODE:
		processing = true;
		if (handleDownloadResponse(response) == -1)
			return -1;
		break;
	}
	return 0;
}

int handleDownloadResponse(string responseString) {
	bitset<BYTE_SIZE> commandBitset(DOWNLOAD_BITSET);
	string response(1, static_cast<unsigned char> (commandBitset.to_ulong()));
	bitset<SIZE_BITSET_SIZE> downloadSizeBitset;
	bitset<DOWNLOAD_BITSET_SIZE_SERVER> responseBitset = getBitset40FromString(responseString.substr(0, 5));

	for (int i = 0; i < SIZE_BITSET_SIZE; i++) {
		downloadSizeBitset[i] = responseBitset[i + 5];
	}

	unsigned long long downloadSize = downloadSizeBitset.to_ullong();
	
	currentPackageNum = 0;
	currentSize = 0;

	if (requestPackage(currentPackageNum) == -1)
		return -1;

	return getFile(downloadSize);
}

int getFile(unsigned long long size) {
	fd_set set;
	struct timeval timeout;
	timeout.tv_sec = SOCKET_READ_TIMEOUT;
	timeout.tv_usec = 0;
	bool errorFlag = false;
	string package = "";
	int packageSize = MIN_PACKAGE_SIZE;
	char buffer[MAX_BLOCK_SIZE];

	if (!downloadFile.is_open()) {
		processing = true;
		if (currentPackageNum == 0)
			downloadFile.open(downloadFileName, ios::out | ios::binary);
		else
			downloadFile.open(downloadFileName, ios::out | ios::binary | ios::app);
	}

	while (true) {
		int receivedBytes = recv(clientSocket, buffer, MAX_BLOCK_SIZE, 0);
		if (errorFlag) {
			continue;
		}

		//

		package.append(&buffer[0], receivedBytes);

		if (package.length() >= PACKAGE_SIZE_SIZE) {
			packageSize = getPackageSizeFromString(package.substr(0, PACKAGE_SIZE_SIZE));
			if (package.length() >= packageSize) {
				switch (checkPackageNum(package, currentPackageNum)) {
				case -1:
					package.erase(0, packageSize);
					continue;
				case 1:
					errorFlag = true;
					continue;
				}
				writePackageToFile(package, packageSize);
				currentPackageNum++;
				currentSize += packageSize - 5;
				package.erase(0, packageSize);
				if (currentSize >= size - 1) {
					downloadFile.close();
					if (requestPackage(currentPackageNum) == -1) {
						return -1;
					}
					processing = false;
					currentPackageNum = 0;
					downloadFile.clear();
					return 0;
				}
			}
		}
	}
	return -1;
}

void writePackageToFile(string package, u_long packageSize) {
	int dataBlockSize = packageSize - 5;
	downloadFile.write(package.substr(5, dataBlockSize).c_str(), dataBlockSize);
}

int checkPackageNum(string package, u_long currentPackageNum) {
	u_long packageNum = getPackageNumFromString(package.substr(2, 3));
	if (packageNum < currentPackageNum)
		return -1;
	if (packageNum > currentPackageNum)
		return 1;
	return 0;
}

u_long getPackageNumFromString(string packageNumString) {
	bitset<BYTE_SIZE * 3> packageNumBitset = getBitset24FromString(packageNumString);
	return packageNumBitset.to_ulong();
}

int getPackageSizeFromString(string packageSizeString) {
	bitset<BYTE_SIZE * PACKAGE_SIZE_SIZE> packageSizeBitset = getBitset16FromString(packageSizeString);

	int packageSize = 0;
	int temp = 1;
	for (int i = 15; i >= 0; i--) {
		packageSize += packageSizeBitset[i] * temp;
		temp *= 2;
	}

	return packageSize;
}

bitset<16> getBitset16FromString(string string) {
	bitset<16> result;
	for (int i = 0; i < 2; i++) {
		bitset<8> byteBitset(string.at(i));
		for (int j = 0; j < 8; j++) {
			result[i * 8 + j] = byteBitset[j];
		}
	}
	return result;
} 

int requestPackage(int currentPackageNum) {
	bitset<BYTE_SIZE> commandBitset(DOWNLOAD_BITSET);
	string request(1, static_cast <unsigned char> (commandBitset.to_ulong()));
	request += getStringFromPackageNum(currentPackageNum);
	return send(clientSocket, request.c_str(), request.length(), 0);
}

bitset<40> getBitset40FromString(string string) {
	bitset<40> result;
	const char* charString = string.c_str();
	for (int i = 0; i < 5; i++) {
		bitset<8> charBitset(charString[i]);
		for (int j = 0; j < 8; j++) {
			result[i * 8 + j] = charBitset[j];
		}
	}
	return result;
}

u_long getUploadPackageNum(string response) {
	string packageNumString = response.substr(1, 3);
	bitset<PACKAGE_NUM_SIZE_SERVER> packageNumBitset = getBitset24FromString(packageNumString);
	u_long packageNum = packageNumBitset.to_ulong();
	return packageNum;
}

int handleUploadResponse(string response) {
	if (!uploadFile.is_open()) {
		processing = true;
		uploadFile.open(uploadFileName, ios::binary | ios::in);
		uploadBlockCount = uploadFileSize / MAX_DATA_BLOCK_SIZE + 1;
	}

	u_long packageNum = getUploadPackageNum(response);

	if (packageNum >= uploadBlockCount) {
		uploadFile.close();
		processing = false;
		return 0;
	}

	uploadFile.seekg(packageNum * (MAX_DATA_BLOCK_SIZE), ios::beg);

	char fileBlock[MAX_DATA_BLOCK_SIZE + 1];

	for (u_long currentPackageNum = packageNum; currentPackageNum < uploadBlockCount; currentPackageNum++) {
		
		cout << currentPackageNum << "/" << uploadBlockCount << endl;

		if (currentPackageNum == uploadBlockCount - 1) {
			memset(fileBlock, 0, MAX_DATA_BLOCK_SIZE + 1);
		}

		uploadFile.read(&fileBlock[0], MAX_DATA_BLOCK_SIZE);
		fileBlock[MAX_DATA_BLOCK_SIZE] = '\0';
		long dataBlockSize = currentPackageNum == uploadBlockCount - 1 ? getDataBlockSize(fileBlock) 
																	: MAX_DATA_BLOCK_SIZE;

		if (dataBlockSize > MAX_DATA_BLOCK_SIZE) {
			dataBlockSize = MAX_DATA_BLOCK_SIZE;
		}

		long packageLength = dataBlockSize + PACKAGE_NUM_SIZE_CLIENT + PACKAGE_SIZE_SIZE;

		char* pack = new char[packageLength];

		string packageLengthString = getStringFromPackageSize(packageLength);
		string packageNumString = getStringFromPackageNum(currentPackageNum);

		memcpy(&pack[0], packageLengthString.c_str(), PACKAGE_SIZE_SIZE);
		memcpy(&pack[2], packageNumString.c_str(), PACKAGE_NUM_SIZE_CLIENT);
		memcpy(&pack[5], &fileBlock[0], dataBlockSize);

		if (send(clientSocket, &pack[0], packageLength, 0) == -1) {
			closesocket(clientSocket);	
			clientSocket = socket(AF_INET, SOCK_STREAM, 0);
			reconnect();
			return 0;
		}

		
		delete[] pack;
	}
	processing = true;
	return 0;
}

bool reconnect() {
	if (!connect(clientSocket, (sockaddr *)&serverAddress, sizeof(serverAddress))) {
		readHello();
		string request = "UPLOAD " + uploadFileName;
		sendRequest(request);
		processing = true;
		return true;
	}
	else {
		cout << "Try again? (Y\N): ";
		string answer;
		getline(cin, answer);
		if (!answer.compare("Y")) {
			return reconnect();
		}
		else {
			processing = false;
			return false;
		}
	}
}

long getDataBlockSize(char* fileBlock) {
	long i = MAX_DATA_BLOCK_SIZE;
	while (fileBlock[i--] == '\0');
	return i + 2;
}

bitset<24> getBitset24FromString(string string) {
	bitset<24> result;
	for (int i = 0; i < 3; i++) {
		bitset<BYTE_SIZE> byteBitset(string.at(i));
		for (int j = 0; j < 8; j++) {
			result[i * 8 + j] = byteBitset[j];
		}
	}
	return result;

	/*bitset<24> result;
	memcpy(&result[0], &string[0], 3);
	return result;*/
}


string getStringFromPackageNum(u_long packageNum) {
	bitset<BYTE_SIZE * 3> packageNumBitset(packageNum);
	return getStringFromBitset24(packageNumBitset);
}

string getStringFromPackageSize(size_t packageSize) {
	bitset<BYTE_SIZE * 2> packageSizeBitset;

	int i = 15;
	while (packageSize != 0) {
		packageSizeBitset[i--] = packageSize % 2;
		packageSize /= 2;
	}
	
	return getStringFromBitset16(packageSizeBitset);
}

int handleTimeResponse(string response) {
	writeMessageFromServer(response.substr(1, TIME_STRING_SIZE));
	return 0;
}

int handleCloseResponse() {
	disconnect();
	return 0;
}

void writeMessageFromServer(string message) {
	cout << "server -> client: " << message << endl;
}

int getResponseCommandPartLength(u_long commandCode) {
	switch (commandCode) {
	case ECHO_CODE:
		return ECHO_SIZE_SERVER;
	case TIME_CODE:
		return TIME_SIZE_SERVER;
	case CLOSE_CODE:
		return CLOSE_SIZE_SERVER;
	case UPLOAD_CODE: 
		return UPLOAD_SIZE_SERVER;
	case DOWNLOAD_CODE:
		return DOWNLOAD_SIZE_SERVER;
	default: 
		return -1;
	}
}

u_long getResponseCommandCode(char commandPart) {
	bitset<BYTE_SIZE> command(commandPart);
	if (command[0] == 0)
		return -1;
	bitset<COMMAND_CODE_SIZE> commandCodeBitset;
	for (int i = 0; i < 4; i++)
		commandCodeBitset[i] = command[i + 1];

	return commandCodeBitset.to_ulong();
}

int readHello() {
	char buffer[MAX_BLOCK_SIZE] = { '\0' };
	int nSize;
	string helloMessage = "";

	while(true) {
		nSize = recv(clientSocket, &buffer[0], sizeof(buffer) - 1, 0);
		for (int i = 0; i < nSize; i++) {
			if (buffer[i] == '\n') {
				cout << helloMessage << endl;
				return 0;
			}

			helloMessage += buffer[i];
		}
	}
	return -1;
}

int getCommandCode(string request) {
	string command = "";
	int i = 0;
	do {
		command += request.at(i++);
	} while (i < request.length() && request.at(i) != ' ');

	if (!command.compare("TIME"))
		return TIME_CODE;
	if (!command.compare("ECHO"))
		return ECHO_CODE;
	if (!command.compare("CLOSE"))
		return CLOSE_CODE;
	if (!command.compare("UPLOAD"))
		return UPLOAD_CODE;
	if (!command.compare("DOWNLOAD"))
		return DOWNLOAD_CODE;
	return -1;
}

unsigned long long getEchoSize(string request) {
	int firstSpace = request.find_first_of(" ");
	return request.length() - firstSpace - 1;
}

string getParameter(string request) {
	int firstSpace = request.find_first_of(" ");
	return request.substr(firstSpace + 1, request.length() - firstSpace - 1);
}

long getFileSize(string fileName) {
	struct stat stat_buf;
	int rc = stat(fileName.c_str(), &stat_buf);
	return rc == 0 ? stat_buf.st_size : -1;
}

string getStringFromBitset40(bitset<40> stringBitset) {
	char* result = new char[5];
	memcpy(&result[0], &stringBitset, 5);
	return string(result, 5);
}

string getStringFromBitset16(bitset<16> stringBitset) {
	char* result = new char[2];
	memcpy(&result[0], &stringBitset, 2);
	return string(result, 2);
}

string getStringFromBitset24(bitset<24> stringBitset) {
	char* result = new char[3];
	memcpy(&result[0], &stringBitset, 3);
	return string(result, 3);
}

string getEchoCommandPart(string request) {
	bitset<ECHO_BITSET_SIZE_CLIENT> commandBitset(ECHO_BITSET);
	bitset<SIZE_BITSET_SIZE> parameterSize(getEchoSize(request));
	for (int i = 0; i < parameterSize.size(); i++)
		commandBitset[i + 5] = parameterSize[i];
	return getStringFromBitset40(commandBitset);
}

string getTimeCommandPart() {
	bitset<TIME_BITSET_SIZE_CLIENT> commandBitset(TIME_BITSET);
	return string(1, static_cast<unsigned char> (commandBitset.to_ulong()));
}

string getCloseCommandPart() {
	bitset<CLOSE_BITSET_SIZE_CLIENT> commandBitset(CLOSE_BITSET);
	return string(1, static_cast<unsigned char> (commandBitset.to_ulong()));
}

string getUploadCommandPart(string request) {
	bitset<UPLOAD_BITSET_SIZE_CLIENT> commandBitset(UPLOAD_BITSET);
	uploadFileName = getParameter(request);
	uploadFileSize = getFileSize(uploadFileName);
	bitset<SIZE_BITSET_SIZE> fileSize(uploadFileSize);
	for (int i = 0; i < fileSize.size(); i++)
		commandBitset[i + 5] = fileSize[i];
	return getStringFromBitset40(commandBitset);
}

string getDownloadCommandPart() {
	bitset<DOWNLOAD_BITSET_SIZE_CLIENT> commandBitset(DOWNLOAD_BITSET);
	return string(1, static_cast<unsigned char> (commandBitset.to_ulong()));
}

string getCommandPart(int commandCode, string request) {
	switch (commandCode) {
	case ECHO_CODE:
		return getEchoCommandPart(request);
	case TIME_CODE:
		return getTimeCommandPart();
	case CLOSE_CODE:
		return getCloseCommandPart();
	case UPLOAD_CODE:
		return getUploadCommandPart(request);
	case DOWNLOAD_CODE:
		return getDownloadCommandPart();
	default:
		return "";
	}
}

string getParameterPart(int commandCode, string request) {
	string result;
	switch (commandCode) {
	case UPLOAD_CODE:
	case DOWNLOAD_CODE:
		downloadFileName = getParameter(request);
		result = downloadFileName;
		while (result.length() < FILE_NAME_SIZE)
			result.push_back('\0');
		return result;
	default:
		return "";
	}
}

int sendRequest(string request) {
	if (request.length() == 0)
		return -1;
	int commandCode;
	if ((commandCode = getCommandCode(request)) == -1)
		return -1;
	
	string commandPart;
	if ((commandPart = getCommandPart(commandCode, request)) == "")
		return -1;
	
	string parameterPart;
	parameterPart = getParameterPart(commandCode, request);

	string message = commandPart + parameterPart;
	send(clientSocket, message.c_str(), message.length(), 0);

}


