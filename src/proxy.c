#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "proxy_parse.h"
#include <netinet/tcp.h>
#include <netdb.h>

#define PROXY_PORT 8080				  //端口号
#define SERVER_PORT 80				  //目标端口
#define SERVER_ADDR "www.example.com" //访问地址

int socket_connect(char *host, in_port_t port)
{
	struct hostent *hp;
	struct sockaddr_in addr;
	int on = 1, sock;

	if ((hp = gethostbyname(host)) == NULL)
	{
		herror("gethostbyname");
		exit(1);
	}
	bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	addr.sin_port = htons(port);
	addr.sin_family = AF_INET;
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&on, sizeof(int));

	if (sock == -1)
	{
		perror("setsockopt");
		exit(1);
	}

	if (connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
	{
		perror("connect");
		exit(1);
	}
	return sock;
}

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("请提供一个端口\n");
		return 0;
	}

	int listenfd, connfd, serverfd;
	int proxy_port;

	/*
	  struct sockaddr_in {
			short   sin_family;
			u_short sin_port;
			struct  in_addr sin_addr;
			char    sin_zero[8];
	};
	*/

	struct sockaddr_in proxy_addr, client_addr, server_addr; // Socket地址
	socklen_t client_len;									 // 用户地址长度
	int n;													 // 请求长度

	//建立一个监听socket
	// AF_INET 使用象 192.9.200.10 这样被点号隔开的四个十进制数字的地址格式。
	// sock_stream 是有保障的(即能保证数据正确传送到对方)面向连接的SOCKET，多用于资料(如文件)传送。
	// sock_dgram 是无保障的面向消息的socket ， 主要用于在网络上发广播信息。

	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd == -1)
	{
		perror("socket");
		exit(1);
	}

	//初始化
	memset(&proxy_addr, 0, sizeof(proxy_addr));
	proxy_port = atoi(argv[1]);
	proxy_addr.sin_family = AF_INET;
	proxy_addr.sin_port = htons(proxy_port);

	//INADDR_ANY是指的自己的IP地址
	proxy_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listenfd, (struct sockaddr *)&proxy_addr, sizeof(proxy_addr)) == -1)
	{
		perror("bind");
		exit(1);
	}

	//最多允许100个链接
	if (listen(listenfd, 100) == -1)
	{
		perror("listen");
		exit(1);
	}
	printf("正在监听端口：%d\n", proxy_port);

	while (1)
	{
		client_len = sizeof(client_addr);
		connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
		if (connfd == -1)
		{
			perror("accept");
			continue;
		}
		int ret = fork();
		//如果是子进程就退出循环
		if (ret == 0)
		{
			break;
		}
		close(connfd);
	}

	printf("接受一个连接来自 %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	close(listenfd);

	char buf[8192], server_host[2048];
	int buflen = 8191;
	while (1)
	{
		n = read(connfd, buf + strlen(buf), buflen);
		char *index = strstr(buf, "\r\n\r\n");
		if (index)
		{
			break;
		}
	}

	if (n < 0)
	{
		printf("获取请求失败\n");
		exit(0);
	}

	int rlen = strlen(buf);
	struct ParsedRequest *req = ParsedRequest_create();
	if (ParsedRequest_parse(req, buf, rlen) < 0)
	{
		printf("请求错误 (400)\n");
		exit(0);
	}

	//如果请求不是get
	if (strcmp(req->method, "GET"))
	{
		printf("未实现(501)\n");
	}

	// 3. Re-format the request to relative:
	ParsedHeader_set(req, "Host", req->host);
	ParsedHeader_set(req, "Connection", "close");
	strcpy(server_host, req->host);
	req->host = "";
	req->protocol = "";
	bzero(buf, 8192);
	ParsedRequest_unparse(req, buf, 8192);
	printf("The buffer is:\n%s", buf);

	// 4. Forward the request to the server:
	// int spsockfd, spportno;
	// struct sockaddr_in sp_addr;
	// char ip_addr[1024];

	// // opening the socket to the server:
	// spsockfd = socket(AF_INET, SOCK_STREAM, 0);
	// if(spsockfd < 0){
	//   printf("Error in opening proxy socket to the server...\n");
	//   return 0;
	// }
	// spportno = 80;

	// // get the url address using the hostname:
	// lookup_host(serv_host, ip_addr);
	// printf("The host address is:\n%s\n", ip_addr);

	// // initialize the proxy server address:
	// bzero((char *) &sp_addr, sizeof(sp_addr));
	// sp_addr.sin_family = AF_INET;
	// sp_addr.sin_addr.s_addr = inet_addr(ip_addr);
	// sp_addr.sin_port = htons(spportno);

	// printf("The buffer length is: %ld\n", strlen(buf)+1);
	// n = write(spsockfd, buf, strlen(buf)+1);
	int spportno = 80;
	int spfd = socket_connect(server_host, spportno);

	write(spfd, buf, strlen(buf)); // write(fd, char[]*, len);
	if (n < 0)
	{
		printf("Error in sending message to url server...\n");
		return 0;
	}
	else
	{
		// printf("Sent request to the server from proxy...\n");
	}

	// 5. Read the reply from the server:
	bzero(buf, 8192);

	while (read(spfd, buf, 8191) != 0)
	{
		// fprintf(stdout, "%s", buf);
		write(connfd, buf, strlen(buf));
		bzero(buf, 8192);
	}
}