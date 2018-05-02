#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/stat.h>

#define BUF_SIZE 1024
#define MAX_FILE_NAME 256
#define MAX_FILE_PATH 256
#define MAX_IPv4 15 //255.255.255.255
#define MAX_PORT 5  //65336

enum{
	UPLOAD=1 , DOWNLOAD=2
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
	unsigned int clientIp;
	unsigned short clientPort;

}clientAddress;

typedef struct{
	int sock;
	char downloadFilePath[MAX_FILE_PATH];
	TorrentFile torrentFile;
}DownloadInfo;

void menu();
void error_handling(char* message);
void makeTorrentFile();
void* listenProc(void* arg);
void* uploadProc(void* arg);
void downloadReady();
void* downloadProc(void* arg);
char* strrev(char *buf);

int serv_flag=0;

int main(int argc, char* argv[])
{
	int num;
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

void menu(){
	puts("==================== Hi Client! ====================");
	puts("1. I want to upload the file");
	puts("2. I want to download the file");
	puts("0. exit");
}

void error_handling(char* message){
	fputs(message,stderr);
	fputc('\n',stderr);
}

void makeTorrentFile()
{
	char filePath[MAX_FILE_PATH]; 
	char trackerIp[MAX_IPv4+1];
	char trackerPort[MAX_PORT+1];
	char temp[MAX_FILE_PATH];
	char hashValue[4];
	char *ptr=NULL;
	int i=0, index;
	char messageType;
	struct stat file_info;
	TorrentFile torrentFile;
	int sock,recv_len;
	struct sockaddr_in serv_addr;
	FILE *file = NULL;
	pthread_attr_t attr;
	pthread_t t_id;

	for(i=0;i<MAX_FILE_PATH;i++)
	{
		filePath[i]=0;
		temp[i]=0;
		torrentFile.filePath[i]=0;
	}

	for(i=0;i<MAX_FILE_NAME;i++)
		torrentFile.realFileName[i]=0;

	for(i=0;i<MAX_IPv4+1;i++)
		trackerIp[i]=0;

	for(i=0;i<MAX_PORT+1;i++)
		trackerPort[i]=0;

	while(1)
	{
		printf("Input the absolute path of the exist file :");
		fgets(filePath,sizeof(filePath),stdin);
		filePath[strlen(filePath)-1] = 0;
		if(access(filePath,0) ==-1)
		{
			puts("Failure to find file");
			continue;
		}
		break;
	}
	
	strcpy(torrentFile.filePath,filePath);
	
	strcpy(temp,filePath);
	ptr = strrev(temp);

	for(i=0;i<strlen(filePath);i++)
	{
		if(ptr[i] == '/')
		{
			i+=1;
			break;
		}
	}
	index = strlen(filePath) -i+1;
	strcpy(torrentFile.realFileName,&filePath[index]);
	
	stat(filePath,&file_info);
	
	torrentFile.bytes = (unsigned int)file_info.st_size;

	strcpy(torrentFile.filePath,filePath);
	
	strcat(filePath,".torrent\0");

	printf("Input tracker server Ip, port number :");
	scanf("%s",trackerIp);
	scanf("%s",trackerPort);

	torrentFile.trackerIp = inet_addr(trackerIp);
	torrentFile.trackerPort = htons(atoi(trackerPort));

	sock = socket(PF_INET,SOCK_STREAM,0);
	
	if(sock == -1)
	{
		error_handling("socket() error");
		return;
	}	

	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = torrentFile.trackerIp;
	serv_addr.sin_port = torrentFile.trackerPort;

	if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1)
	{
		close(sock);
		error_handling("connect() error");
	 	return;
	}
	printf("Server Connected\n");
	messageType = UPLOAD;

	if(write(sock,&messageType,sizeof(messageType)) == -1)
	{
		close(sock);
		error_handling("send() error");
		return;
	}
	
	if(read(sock,hashValue,sizeof(hashValue) ==-1))
	{
		close(sock);
		error_handling("recv() error");
		return;
	}
	torrentFile.hashValue = (*(int*)hashValue);
	file = fopen(filePath,"wb");
	fwrite((void*)&torrentFile,1,sizeof(torrentFile),file);
	
	close(sock);
	fclose(file);

	if(!serv_flag)	
	{
		if(pthread_attr_init(&attr) !=0)
		{	
			error_handling("Failure to make listenProc thread");
			return;
		}
		if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) !=0)
		{	
			error_handling("Failure to make listenProc thread");
			return;
		}
		if(pthread_create(&t_id,&attr,listenProc,NULL) !=0)
		{	
			error_handling("Failure to make listenProc thread");
			return;
		}
		pthread_attr_destroy(&attr);
		printf("listenProc thread start\n");
		serv_flag = 1;
	}
}

void* listenProc(void *arg)
{
	int serv_sock,*clnt_sock;
	struct sockaddr_in serv_addr, clnt_addr;
	int clnt_adr_sz,recv_len;
	char* ptr;
	pthread_attr_t attr;
	pthread_t t_id;	
	serv_sock = socket(PF_INET,SOCK_STREAM,0);

	if(serv_sock == -1)
	{
		error_handling("--------------------listenProc error--------------------");
		error_handling("socket() error");
		serv_flag = 0;
		return;
	}
	
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(8000);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(serv_sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1)
	{
		close(serv_sock);
		error_handling("--------------------listenProc error--------------------");
		error_handling("bind() error");
		serv_flag = 0;
		return;
	}
	
	if(listen(serv_sock,15) == -1)
	{
		close(serv_sock);
		error_handling("-------------------listenProc error--------------------");
		error_handling("listen() error");
		serv_flag = 0;
		return;

	}

	if(pthread_attr_init(&attr) !=0)
	{
		close(serv_sock);
		error_handling("--------------------listenProc error--------------------");
		error_handling("pthread_attr_init() error");
		serv_flag=0;
		return;
	}
	
	
	if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) !=0)
	{
		close(serv_sock);
		pthread_attr_destroy(&attr);
		error_handling("--------------------listenProc error--------------------");
		error_handling("pthread_attr_setdetachstate() error");
		serv_flag=0;
		return;
	}

	while(1)
	{
		clnt_adr_sz = sizeof(clnt_addr);
		clnt_sock = (int*)malloc(sizeof(int));
		*clnt_sock  = accept(serv_sock,(struct sockaddr*)&clnt_addr,&clnt_adr_sz);
		
		if(*clnt_sock == -1)
			continue;
		
		if(pthread_create(&t_id,&attr,uploadProc,(void*)clnt_sock) !=0)
			continue;
		printf("Upload start(uploadProc thread is made)\n");
		printf("uploadProc connect Ip: %s\n",inet_ntoa(clnt_addr.sin_addr));
	}
}

void* uploadProc(void *arg)
{
	int *clnt_sock = (int*)arg;
	FILE*file = NULL;
	char filePath[MAX_FILE_PATH];
	char buf[BUF_SIZE];
	int recv_len,i;
	unsigned int foffset;
	
	for(i=0;i<MAX_FILE_PATH;i++)
		filePath[i]=0;
	for(i=0;i<BUF_SIZE;i++)
		buf[i]=0;

	if(read(*clnt_sock,filePath,sizeof(filePath)) == -1)
	{
		close(*clnt_sock);
		free(clnt_sock);
		return;
	}
	
	filePath[strlen(filePath)] = 0;
	
	

	if(access(filePath,0) == -1)
	{
		close(*clnt_sock);
		free(clnt_sock);
		return;
	}
	
	if(read(*clnt_sock,&foffset,sizeof(foffset)) == -1)
	{
		close(*clnt_sock);
		free(clnt_sock);
		return;

	}
	file = fopen(filePath,"rb");
	fseek(file,foffset,SEEK_SET);
	
	while(1)
	{
		if((recv_len = fread(buf,1,sizeof(buf),file)) == -1)
			break;
		if(write(*clnt_sock,buf,recv_len) == -1)
		{
			close(*clnt_sock);
			fclose(file);
			return;
		}
		if(recv_len <sizeof(buf))
			break;
	}
	printf("uploadProc complete\n");
	close(*clnt_sock);
	free(clnt_sock);
	fclose(file);
}

void downloadReady()
{
	char torrentFilePath[MAX_FILE_PATH];
	char messageType;
	DownloadInfo* downloadInfo;
	clientAddress clnt_addr;
	FILE *file = NULL;
	int sock,recv_len,i;
	struct sockaddr_in serv_addr;
	pthread_attr_t attr;
	pthread_t t_id;

	downloadInfo = (DownloadInfo*)malloc(sizeof(DownloadInfo));

	for(i=0;i<MAX_FILE_PATH;i++)
	{
		torrentFilePath[i]=0;
		downloadInfo->downloadFilePath[i]=0;
		downloadInfo->torrentFile.filePath[i]=0;
	}

	for(i=0;i<MAX_FILE_NAME;i++)
		downloadInfo->torrentFile.realFileName[i]=0;
	while(1)
	{
		printf("Input the absolute path of the exist torrent file :");
		fgets(torrentFilePath,sizeof(torrentFilePath),stdin);
		torrentFilePath[strlen(torrentFilePath)-1] = 0;
		if(access(torrentFilePath,0) == -1)
		{
			puts("Failure to find file");
			continue;
		}
		break;
	}
		
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
	file = fopen(torrentFilePath,"rb");
	fread((void*)&downloadInfo->torrentFile,1,sizeof(downloadInfo->torrentFile),file);
	
	if(downloadInfo->downloadFilePath[strlen(downloadInfo->downloadFilePath)] != '/')
	{	
		strcat(downloadInfo->downloadFilePath,"/\0");
	}

	strcat(downloadInfo->downloadFilePath,downloadInfo->torrentFile.realFileName);
	
	sock = socket(PF_INET,SOCK_STREAM,0);
	
	if(sock == -1)
	{
		free(downloadInfo);
		error_handling("socket() error");
		return;
	}
	
	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = downloadInfo->torrentFile.trackerIp;
	serv_addr.sin_port = downloadInfo->torrentFile.trackerPort;
	
	if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1)
	{
		close(sock);
		free(downloadInfo);
		error_handling("connect() error");
		return;
	}
	messageType = DOWNLOAD;
	
	if(write(sock,&messageType,sizeof(messageType)) == -1) 
	{
		close(sock);
		free(downloadInfo);
		error_handling("write() error");
		return;
	}
	
	if(write(sock,&downloadInfo->torrentFile.hashValue,sizeof(downloadInfo->torrentFile.hashValue)) == -1)
	{
		close(sock);
		free(downloadInfo);
		error_handling("write() error");
		return;
	}
	
	if(read(sock,&clnt_addr,sizeof(clnt_addr)) == -1)
	{
		close(sock);
		free(downloadInfo);
		error_handling("read() error");
		return;
	}
	close(sock);

	sock = socket(PF_INET,SOCK_STREAM,0);

	if(sock == -1)
	{
		close(sock);
		free(downloadInfo);
		error_handling("socket() error");
		return;
	}
	serv_addr.sin_addr.s_addr = clnt_addr.clientIp;
	serv_addr.sin_port = clnt_addr.clientPort;
		
	if(connect(sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1)
	{
		close(sock);
		free(downloadInfo);
		error_handling("connect() error");
		return;
	} 

	if(write(sock,downloadInfo->torrentFile.filePath,strlen(downloadInfo->torrentFile.filePath)) == -1)
	{
		close(sock);
		free(downloadInfo);
		error_handling("write() error");
		return;
	}

	downloadInfo->sock = sock;

	if(pthread_attr_init(&attr) !=0)
	{
		close(sock);
		free(downloadInfo);

		error_handling("Failure to make downloadProc thread");
		return;
	}
	if(pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) !=0)
	{	
		close(sock);
		free(downloadInfo);
		error_handling("Failure to make downloadProc thread");
		return;
	
	}
	if(pthread_create(&t_id,&attr,downloadProc,(void*)downloadInfo) !=0)
	{	
		close(sock);
		free(downloadInfo);
		error_handling("Failure to make downloadProc thread");
		return;
	}
	pthread_attr_destroy(&attr);
	printf("Download Start(downloadProc Thread is made)\n");
}

void* downloadProc(void* arg)
{
	DownloadInfo *downloadInfo = (DownloadInfo*)arg;
	FILE *file = NULL;
	char buf[BUF_SIZE];
	int recv_len,i; 
	struct stat file_info;
	unsigned int foffset;

	for(i=0;i<BUF_SIZE;i++)
		buf[i]=0;	
	file = fopen(downloadInfo->downloadFilePath,"ab");
	
	stat(downloadInfo->downloadFilePath,&file_info);

	foffset = (unsigned int)file_info.st_size;
	
	if(write(downloadInfo->sock,&foffset,sizeof(foffset)) == -1)	
	{
		close(downloadInfo->sock);
		fclose(file);
		free(downloadInfo);
		error_handling("--------------------downloadProc error--------------------");
		error_handling("write() error");
		return;
	}

	while((recv_len = read(downloadInfo->sock,buf,sizeof(buf))) !=0)
	{	
		
		if(recv_len == -1)
		{
			close(downloadInfo->sock);
			fclose(file);
			free(downloadInfo);
			error_handling("--------------------downloadProc error--------------------");
			error_handling("read() error");
			return;
		}
		fwrite(buf,1,recv_len,file);
		if(fflush(file) == EOF)
		{
			close(downloadInfo->sock);
			fclose(file);
			free(downloadInfo);
			error_handling("--------------------downloadProc error--------------------");
			error_handling("fflush() error");
			return;
		}
	}
	puts("--------------------Message from downloadProc-------------------- ");
	puts("Download complete");
	close(downloadInfo->sock);
	free(downloadInfo);
	fclose(file);
}
char* strrev(char* buf){
	int i=0;
	char*temp = (char*)malloc(strlen(buf));
	strcpy(temp,buf);
	temp[strlen(temp)] = 0;

	for(i=strlen(buf)-1;i>=0;i--)
	{
		buf[(strlen(buf)-1)-i] = temp[i];
	}
	buf[strlen(buf)]= 0;
	free(temp);
	return buf;
}

