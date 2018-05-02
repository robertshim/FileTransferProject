#include<stdio.h>
#include<io.h>
#include<tchar.h>
#include<WinSock2.h>
#include<stdlib.h>
#include<process.h>

#define BUF_SIZE 1024
#define MAX_FILE_NAME 256
#define MAX_FILE_PATH 256
#define MAX_IPv4 15 //255.255.255.255
#define MAX_PORT 5  //65336

enum{
		UPLOAD = 1, DOWNLOAD=2
};
typedef struct{
	unsigned int hashValue;
	unsigned int bytes;
	unsigned int trackerIp;
	unsigned short trackerPort;
	char realFileName[MAX_FILE_NAME];
	char filePath[MAX_FILE_PATH];
}TorrentFile;

typedef struct{
	unsigned int clinetIp;
	unsigned short clientPort;
}clientAddress;

typedef struct{
	SOCKET sock;	
	char downloadFilePath[MAX_FILE_PATH];
	TorrentFile torrentFile;
}DownloadInfo;

void menu();
void error_handling(char* message);
void makeTorrentFile();
unsigned int WINAPI listenProc(void *lpParam);
unsigned int WINAPI uploadProc(void* lpParam);
void downloadReady();
unsigned int WINAPI downloadProc(void *lpParam);

int serv_flag = 0;

int main(int argc,char* argv[]){
	int num;
	WSADATA wsaData;

	if(WSAStartup(MAKEWORD(2,2),&wsaData) !=0)
	{	
		error_handling("WSAStartup() error");
		exit(0);
	}
	while(1)
	{
		menu();
		scanf("%d",&num);
		getchar();
		switch(num){
		case 0:
			exit(0);
		case 1:
			makeTorrentFile();
			break;
		case 2:
			downloadReady();
			break;
		default:
			puts("This is not exist number");
		}
	}
	return 0;
}

void menu()
{
	puts("==================== Hi Client! ====================");
	puts("1. I want to upload the file");
	puts("2. I want to download the file.");
	puts("0. exit");
}

void error_handling(char* message){
	fputs(message,stderr);
	fputc('\n',stderr);
}

//웹에 업로드할 씨드 파일을 만듭니다.
void makeTorrentFile()
{
	char filePath[MAX_FILE_PATH];
	char temp[MAX_FILE_PATH];
	char trackerIp[MAX_IPv4+1];
	char trackerPort[MAX_PORT+1];
	char hashValue[4];
	char *ptr=NULL;
	int i=0;
	int index,recv_len;
	char messageType;
	FILE *file = NULL;
	TorrentFile torrentFile;
	SOCKET sock;
	SOCKADDR_IN serv_addr;


	//초기화 과정
	for(i=0;i<MAX_FILE_PATH;i++)
	{	
		filePath[i]=0;
		temp[i]=0;
		torrentFile.filePath[i]=0;
	}

	for(i=0;i<MAX_IPv4+1;i++)
	{
		trackerIp[i] = 0;
	}

	for(i=0;i<MAX_FILE_NAME;i++)
	{
		torrentFile.realFileName[i] = 0;
	}

	//씨드파일을 만들 대상 파일을 고릅니다.
	while(1)
	{
		//파일의 절대 경로를 입력
		printf("Input the absolute path of the exist file :");
		fgets(filePath,sizeof(filePath),stdin);
		filePath[strlen(filePath)-1]=0;
		if(access(filePath,0) == -1)
		{
			puts("Failure to find file");		
			continue;
		}
		break;
	}

	strcpy(torrentFile.filePath,filePath);
	strcpy(temp,filePath);

	//절대경로에서 파일의 이름만을 추출
	ptr = strrev(temp);

	for(i=0;i<strlen(filePath);i++)
	{	
		if(ptr[i] == '/')
		{	
			i+=1;
			break;
		}
	}
	index = strlen(filePath) - i +1;
	strcpy(torrentFile.realFileName,&filePath[index]);

	//파일 전체 크기를 얻습니다.
	file = fopen(filePath,"rb");
	torrentFile.bytes = file->_bufsiz;
	fclose(file);

	strcpy(torrentFile.filePath,filePath);
	strcat(filePath,".torrent\0");
	
	//트랙커 서버의 정보를 입력
	printf("Input tracker server Ip, port number :");
	scanf("%s",&trackerIp);
	scanf("%s",&trackerPort);

	torrentFile.trackerIp = inet_addr(trackerIp);
	torrentFile.trackerPort = htons(atoi(trackerPort));

	//서버에 연결하려고 준비
	sock = socket(PF_INET,SOCK_STREAM,0);
	
	if(sock==INVALID_SOCKET)
	{
		error_handling("socket() error");
		return;
	}

	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr = torrentFile.trackerIp;
	serv_addr.sin_port = torrentFile.trackerPort;

	if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == SOCKET_ERROR)
	{
		closesocket(sock);
		error_handling("connect() error");
		return;
	}
	printf("Server Connected\n");
	messageType = UPLOAD;

	//자신이 업로더 임을 알림.
	if(send(sock,&messageType,sizeof(messageType),0)  == SOCKET_ERROR)
	{
		closesocket(sock);
		error_handling("send() error");
		return;
	}

	//자신이 등록된 번호를 입력받습니다.
	if(recv(sock,hashValue,sizeof(hashValue),0) == -1)
	{
		closesocket(sock);
		error_handling("recv() error");
		return ;

	}
	torrentFile.hashValue = (*(int*)hashValue);

	//씨드파일 생성
	file = fopen(filePath,"wb");
	fwrite((void*)&torrentFile,1,sizeof(torrentFile),file);
	
	closesocket(sock);
	fclose(file);
	
	//다른 클라이언트에게 업로드를 해줄 스레드 생성.
	if(!serv_flag)
	{
		if(_beginthreadex(NULL,0,listenProc,NULL,0,NULL) == NULL)
		{
			error_handling("Filure to make listenProc thread");
		}
		printf("listenProc thread start\n");
		serv_flag = 1;
	}
}

//다른 클라이언트에게 업로드를 해줄 스레드
unsigned int WINAPI listenProc(void *lpParam){
	SOCKET serv_sock,clnt_sock;
	SOCKADDR_IN serv_addr, clnt_addr;
	int clnt_adr_sz,recv_len;
	char* ptr;

	serv_sock = socket(PF_INET,SOCK_STREAM,0);

	if(serv_sock == INVALID_SOCKET)
	{
		error_handling("-------------------listenProc error--------------------");
		error_handling("socket() error");
		serv_flag = 0;
		return 0;
	}

	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(8000);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(serv_sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == SOCKET_ERROR)
	{
		closesocket(serv_sock);
		error_handling("-------------------listenProc error--------------------");
		error_handling("bind() error");
		serv_flag = 0;
		return 0;
	}

	if(listen(serv_sock,15) == SOCKET_ERROR)
	{
		closesocket(serv_sock);
		error_handling("-------------------listenProc error--------------------");
		error_handling("listen() error");
		serv_flag = 0;
		return 0;
	} 

	while(1)
	{
		clnt_adr_sz = sizeof(clnt_addr);
		//clnt_sock = (SOCKET*)malloc(sizeof(SOCKET));
		clnt_sock = accept(serv_sock,(SOCKADDR*)&clnt_addr,&clnt_adr_sz);
		
		if(clnt_sock == SOCKET_ERROR)
			continue;
		if(_beginthreadex(NULL,0,uploadProc,(void*)&clnt_sock,0,NULL) == NULL)
			continue;
		printf("Upload start(uploadProc thread is made)\n");
		printf("uploadProc connect Ip : %s\n",inet_ntoa(clnt_addr.sin_addr));
	}
}
//클라이언트와 연결되어서 파일을 전송하는 스레드
unsigned int WINAPI uploadProc(void* lpParam){
	SOCKET clnt_sock = (*(SOCKET*)lpParam);
	FILE *file = NULL;
	char filePath[MAX_FILE_PATH];
	char buf[BUF_SIZE];
	int recv_len,foffset,i;

	for(i=0;i<MAX_FILE_PATH;i++)
		filePath[i]=0;
	for(i=0;i<BUF_SIZE;i++)
		buf[i]=0;

	if(recv(clnt_sock,filePath,sizeof(filePath),0) == SOCKET_ERROR )
	{
		closesocket(clnt_sock);
		return 0;
	}

	filePath[strlen(filePath)]=0;
	if(access(filePath,0) == -1)
	{
		closesocket(clnt_sock);
		return 0;
	}
	
	if(recv(clnt_sock,(char*)&foffset,sizeof(foffset),0) == SOCKET_ERROR)
	{
		closesocket(clnt_sock);
		return 0;	
	}

	file = fopen(filePath,"rb");

	fseek(file,foffset,SEEK_SET);

	while(1)
	{
		if((recv_len=fread(buf,1,sizeof(buf),file)) == -1)
			break;
		if(send(clnt_sock,buf,recv_len,0) ==SOCKET_ERROR)
		{
			closesocket(clnt_sock);
			fclose(file);
			return -1;
		}
		//printf("send %d bytes\n",recv_len);
		if(recv_len < sizeof(buf))
				break;
	}
	printf("uploadProc complete\n");
	closesocket(clnt_sock);
	fclose(file);
	return 0;
}

//다운로드를 하기 위해서 준비하는 작업을 담당
void downloadReady(){
	char torrentFilePath[MAX_FILE_PATH];
	char messageType;
	DownloadInfo* downloadInfo;
	FILE *file = NULL;
	int recv_len,i;
	clientAddress clnt_addr;
	SOCKET sock;
	SOCKADDR_IN serv_addr;

	downloadInfo = (DownloadInfo*)malloc(sizeof(DownloadInfo));
	
	for(i=0;i<MAX_FILE_PATH;i++)
	{	
		torrentFilePath[i] = 0;
		downloadInfo->downloadFilePath[i]=0;
		downloadInfo->torrentFile.filePath[i]=0;
	}
	for(i=0;i<MAX_FILE_NAME;i++)
		downloadInfo->torrentFile.realFileName[i]=0;

	//웹에서 다운로드 받은 시드파일의 절대경로를 입력
	while(1)
	{
		printf("Input the path of the exist torrent file :");
		fgets(torrentFilePath,sizeof(torrentFilePath),stdin);
		torrentFilePath[strlen(torrentFilePath)-1]=0;
		if(access(torrentFilePath,0) ==-1)
		{
			puts("Failure to find file");
			continue;
		}
		break;
	}

	//파일을 다운로드 받고자하는 경로를 입력합니다.
	while(1)
	{
		printf("Please input the path download :");
		fgets(downloadInfo->downloadFilePath,sizeof(downloadInfo->downloadFilePath),stdin);
	
		downloadInfo->downloadFilePath[strlen(downloadInfo->downloadFilePath)-1] = 0;
		if(access(downloadInfo->downloadFilePath,0) == -1)
		{
			puts("Failure to find path");
			continue;
		}
		break;
	}
	//씨드파일의 정보를 읽어옵니다.
	file = fopen(torrentFilePath,"rb");
	
	fread((void*)&downloadInfo->torrentFile,1,sizeof(downloadInfo->torrentFile),file);
	
	//다운로드 받을 경로의 마지막에 /가 있는지 없는지를 체크합니다.
	if(downloadInfo->downloadFilePath[strlen(downloadInfo->downloadFilePath)]!='/')
	{
		strcat(downloadInfo->downloadFilePath,"/\0");
	}

	strcat(downloadInfo->downloadFilePath,downloadInfo->torrentFile.realFileName);

	//해당 파일을 가지고 있는 클라이언트의 정보를 수집합니다.
	sock = socket(PF_INET,SOCK_STREAM,0);

	if(sock == INVALID_SOCKET)
	{
		free(downloadInfo);
		error_handling("socket() error");
		return;
	}

	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = downloadInfo->torrentFile.trackerIp;
	serv_addr.sin_port = downloadInfo->torrentFile.trackerPort;

	if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == SOCKET_ERROR)
	{
		free(downloadInfo);
		closesocket(sock);		
		error_handling("connect() error");
		return;
	}
	messageType = DOWNLOAD;

	if(send(sock,&messageType,sizeof(messageType),0) == SOCKET_ERROR)
	{
		free(downloadInfo);
		closesocket(sock);
		error_handling("send() error");
		return;
	}
	
	if(send(sock,(char*)&downloadInfo->torrentFile.hashValue,sizeof(downloadInfo->torrentFile.hashValue),0) == SOCKET_ERROR)
	{
		free(downloadInfo);
		closesocket(sock);		
		error_handling("send() error");
		return;
	}
	if(recv(sock,(char*)&clnt_addr,sizeof(clnt_addr),0) == SOCKET_ERROR)
	{
		free(downloadInfo);
		closesocket(sock);	
		error_handling("recv() error");
		return;
	}

	closesocket(sock);

	//해당파일을 가지고 있는 클라이언트와의 연결준비
	sock = socket(PF_INET,SOCK_STREAM,0);
	
	if(sock == INVALID_SOCKET)
	{
		free(downloadInfo);
		closesocket(sock);
		error_handling("socket() error");
		return;
	}
	serv_addr.sin_addr.s_addr = clnt_addr.clinetIp;
	serv_addr.sin_port = clnt_addr.clientPort;

	if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == SOCKET_ERROR)
	{
		free(downloadInfo);
		closesocket(sock);		
		error_handling("connect() error");
		return;
	}

	if(send(sock,downloadInfo->torrentFile.filePath,strlen(downloadInfo->torrentFile.filePath),0) == SOCKET_ERROR)
	{
		free(downloadInfo);
		closesocket(sock);
		error_handling("send() error");
		return;
	}
	downloadInfo->sock = sock;

	//다운로드를 진행할 스레드를 생성
	if(_beginthreadex(NULL,0,downloadProc,(void*)downloadInfo,0,NULL) == NULL)
	{
		error_handling("Failure to make downloadProc thread");
		return;
	}
	printf("Download Start(downloadProc Thread is made)\n");
}

//파일을 다운로드 받는일을 수행합니다.
unsigned int WINAPI downloadProc(void *lpParam){
	
	DownloadInfo *downloadInfo = (DownloadInfo*)lpParam;
	FILE *file = NULL;
	char buf[BUF_SIZE];
	int recv_len,i;
	unsigned int foffset;
	
	for(i=0;i<BUF_SIZE;i++)
		buf[i]=0;
	
	file = fopen(downloadInfo->downloadFilePath,"ab");
	foffset = (unsigned int)file->_bufsiz;
	if(send(downloadInfo->sock,(char*)&foffset,sizeof(foffset),0) == SOCKET_ERROR)
	{
		closesocket(downloadInfo->sock);
		fclose(file);
		free(downloadInfo);
		error_handling("--------------------downloadProc error--------------------");
		error_handling("write() error");
		return 0;
	}
	while((recv_len=recv(downloadInfo->sock,buf,sizeof(buf),0)) !=0)
	{

		if(recv_len == SOCKET_ERROR)
		{
			closesocket(downloadInfo->sock);
			fclose(file);
			free(downloadInfo);
			error_handling("--------------------downlaodProc error--------------------");
			error_handling("recv() error");
			return 0;
		}
		fwrite(buf,1,recv_len,file);
		if(fflush(file) == EOF)
		{
			closesocket(downloadInfo->sock);
			fclose(file);
			free(downloadInfo);
			error_handling("--------------------downloadProc error--------------------");
			error_handling("fflush() error");
			return 0;
		}
		//printf("receive %d bytes\n",recv_len);
	}
	puts("--------------------Message from downloadProc--------------------");
	printf("Download complete(file name: %s)",downloadInfo->torrentFile.realFileName);
	closesocket(downloadInfo->sock);
	free(downloadInfo);
	fclose(file);
	return 0;
}