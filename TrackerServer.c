#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<signal.h>
#include<sys/socket.h>
#include<fcntl.h>
#include<errno.h>
#include<sys/epoll.h>

#define EPOLL_SIZE 1024
#define BUF_SIZE 1024
#define MAX_NODE_SIZE 1024
#define MAX_UPLOADER 1024

enum{
	UPLOAD = 1, DOWNLOAD = 2 ,EXIT=0
};
typedef struct{
	unsigned int clientIp;
	unsigned short clientPort;
}ClientAddress;

typedef struct{
	ClientAddress clnt_addr;
	int fd;
}Node;

void error_handling(char* message);

int main(int argc,char* argv[])
{
	ClientAddress uploader[MAX_UPLOADER];
	int num_uploader=0;
	char buf[BUF_SIZE],messageType;	
	int serv_sock,clnt_sock;
	struct sockaddr_in serv_addr,clnt_addr;
	socklen_t clnt_addr_sz;
	int str_len,i,count = 0,total=0;
	struct epoll_event *ep_events;
	struct epoll_event event;
	int epfd,event_cnt,flag;
	Node node[MAX_NODE_SIZE];
	int node_flag[MAX_NODE_SIZE];
	
	if(argc!=2)
	{
		printf("Usage %s <port>\n",argv[0]);
		exit(0);
	}
	for(i=0;i<MAX_NODE_SIZE;i++)
	{
		node_flag[i] = 0;
	}
	
	serv_sock = socket(PF_INET,SOCK_STREAM,0);	
	if(serv_sock == -1)
	{	
		perror("socket()");
		exit(0);
	}

	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));

	if(bind(serv_sock,(struct sockaddr*)&serv_addr,sizeof(serv_addr)) == -1)
	{
		perror("bind()");
		exit(0);
	}

	if(listen(serv_sock,15) == -1)
	{
		perror("listen()");
		exit(0);
	}
	
	epfd = epoll_create(EPOLL_SIZE);
	ep_events = malloc(sizeof(struct epoll_event)*EPOLL_SIZE);

	flag = fcntl(serv_sock,F_GETFL,0);
	fcntl(serv_sock,F_SETFL,flag|O_NONBLOCK);

	event.events=EPOLLIN;
	event.data.fd = serv_sock;
	epoll_ctl(epfd,EPOLL_CTL_ADD,serv_sock,&event);

	printf("Start Server\n");	
	while(1)
	{
		event_cnt = epoll_wait(epfd,ep_events,EPOLL_SIZE,-1);
		if(event_cnt == -1)
		{
			puts("epoll_wait() error");
			break;
		}
	
		
		for(i=0;i<event_cnt;i++)
		{
			if(ep_events[i].data.fd == serv_sock)
			{
				clnt_addr_sz = sizeof(clnt_addr);
				clnt_sock = accept(serv_sock,(struct sockaddr*)&clnt_addr,&clnt_addr_sz);
				if(total <= MAX_NODE_SIZE)
				{
					while(1)
					{	
						if(node_flag[count] == 0)
						{	
							node[count].clnt_addr.clientIp = clnt_addr.sin_addr.s_addr;	
							node[count].clnt_addr.clientPort = htons(8000);
							node[count].fd = clnt_sock;
							node_flag[count++] = 1;
							total++;	
							flag = fcntl(clnt_sock,F_GETFL,0);
							fcntl(clnt_sock,F_SETFL,flag|O_NONBLOCK);
							event.events = EPOLLIN | EPOLLET;
							event.data.fd = clnt_sock;
							epoll_ctl(epfd,EPOLL_CTL_ADD,clnt_sock,&event);
							printf("Connect client : %s\n\n",inet_ntoa(clnt_addr.sin_addr));
							break;
						}
						count++;
						count = count % MAX_NODE_SIZE;
					}
			
				}
				else
				{
					close(clnt_sock);
					printf("There are too many waiters");
			
				}
			}
			else
			{
				int j;
				for(j=0;j<MAX_NODE_SIZE;j++)
				{
					if(node_flag[j] == 1)
					{
						if(node[j].fd == ep_events[i].data.fd)
						{		
							break;
						}
					}
				}
				if(read(ep_events[i].data.fd,&messageType,sizeof(messageType)) == -1)
				{
					node_flag[j] = 0;	
					total--;
				
					epoll_ctl(epfd,EPOLL_CTL_DEL,ep_events[i].data.fd,NULL);
					close(ep_events[i].data.fd);
				}
					
				if(messageType == UPLOAD)
				{
					uploader[num_uploader].clientIp = node[j].clnt_addr.clientIp;
					uploader[num_uploader].clientPort = node[j].clnt_addr.clientPort;
					node_flag[j] = 0;
					total--;
					clnt_addr.sin_addr.s_addr = uploader[num_uploader].clientIp;
					printf("Register as uploader : %s\n",inet_ntoa(clnt_addr.sin_addr));
					write(ep_events[i].data.fd,&num_uploader,sizeof(num_uploader));
					num_uploader++;
					epoll_ctl(epfd,EPOLL_CTL_DEL,ep_events[i].data.fd,NULL);
					close(ep_events[i].data.fd);
					printf("Disconnect client : %s\n\n",inet_ntoa(clnt_addr.sin_addr));
				}

				if(messageType == DOWNLOAD)
				{
				
					ClientAddress temp;
					unsigned int hashValue;

					read(ep_events[i].data.fd,&hashValue,sizeof(hashValue));
					
					temp.clientIp = uploader[hashValue].clientIp;			
					temp.clientPort = uploader[hashValue].clientPort;	
						
					write(ep_events[i].data.fd,&temp,sizeof(temp));
					
					clnt_addr.sin_addr.s_addr = node[j].clnt_addr.clientIp;
					
					node_flag[j]=0;
					total--;

					epoll_ctl(epfd,EPOLL_CTL_DEL,ep_events[i].data.fd,NULL);
					close(ep_events[i].data.fd);	
					printf("Disconnect client : %s\n\n",inet_ntoa(clnt_addr.sin_addr));
				}
				
				if(messageType == EXIT)
				{
					clnt_addr.sin_addr.s_addr = node[j].clnt_addr.clientIp;
					node_flag[j]=0;
					total--;
					
					epoll_ctl(epfd,EPOLL_CTL_DEL,ep_events[i].data.fd,NULL);
					close(ep_events[i].data.fd);
					printf("Disconnect client : %s\n\n",inet_ntoa(clnt_addr.sin_addr));
				}
			}
		}
	}
	close(serv_sock);
	close(epfd);	
	return 0;
}

void error_handling(char *message){
	fputs(message,stderr);
	fputc('\n',stderr);
	exit(0);
}
