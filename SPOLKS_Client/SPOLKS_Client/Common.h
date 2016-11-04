#include <iostream>
#include <fstream>
#include <string.h>
#include <string>
#include <bitset>
#pragma comment (lib, "ws2_32.lib")

#include <windows.h>
#include <conio.h>
#include <io.h>

using namespace std;

#define PORT_NUM 2222
#define SERVER_ADDRESS "192.168.30.1"
#define MAX_BLOCK_SIZE 65534
#define SIZE_BITSET_SIZE 35
#define FILE_NAME_SIZE 16
#define BYTE_SIZE 8
#define COMMAND_CODE_SIZE 4
#define TIME_STRING_SIZE 24
#define PACKAGE_NUM_SIZE_SERVER 24
#define PACKAGE_NUM_SIZE_CLIENT 3
#define PACKAGE_SIZE_SIZE 2
#define MAX_DATA_BLOCK_SIZE 65529

#define ECHO_BITSET_SIZE_CLIENT 40
#define TIME_BITSET_SIZE_CLIENT 8
#define CLOSE_BITSET_SIZE_CLIENT 8
#define UPLOAD_BITSET_SIZE_CLIENT 40
#define DOWNLOAD_BITSET_SIZE_CLIENT 8
#define DOWNLOAD_BITSET_SIZE_SERVER 40

#define ECHO_BITSET 0x3
#define TIME_BITSET 0x5
#define CLOSE_BITSET 0x7
#define UPLOAD_BITSET 0x9
#define DOWNLOAD_BITSET 0xB

//from server
#define ECHO_SIZE_SERVER 1
#define TIME_SIZE_SERVER 25
#define CLOSE_SIZE_SERVER 1
#define UPLOAD_SIZE_SERVER 4
#define DOWNLOAD_SIZE_SERVER 5

#define ECHO_CODE 1
#define TIME_CODE 2
#define CLOSE_CODE 3
#define UPLOAD_CODE 4
#define DOWNLOAD_CODE 5

#define SOCKET_READ_TIMEOUT 120

#define MIN_PACKAGE_SIZE 3

char webSocketDetails[1024];
SOCKET clientSocket;
ifstream uploadFile;
ofstream downloadFile;
string uploadFileName;
long uploadFileSize;
long uploadBlockCount;
bool processing;
sockaddr_in serverAddress;

//DOWNLOAD
string downloadFileName;
u_long currentPackageNum;
unsigned long long currentSize;