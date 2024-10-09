#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

#define IP "192.168.10.81"
#define PORT 7777
int sockfd = 0;//定义并初始化

//shou线程接收服务器消息
void* recv_info(void* newfd)
{
    int fd = *(int*)newfd;
    int ret = 0;
    char shou[1024];
    while (1)
    {   
        memset(shou, 0, 1024);
        if ((ret = recv(fd, shou, 1024, 0)) == -1) exit(-1);
        shou[ret] = '\0'; // 确保字符串以 '\0' 结尾  
        
        if (strncmp(shou, "start:\n", 7) == 0)
        {   
            printf("开始接收文件\n");

            //接收文件大小
            memset(shou, 0, 1024);  
            recv(fd, shou, 1023, 0);
            int fileSize = atoi(shou);
            printf("接收到的文件大小是%d\n",fileSize);

            // 接收文件名  
            char filename[256];
            memset(filename, 0, 256);  
            memset(shou, 0, 1024);  
            if ((ret = recv(fd, shou, 1023, 0)) <= 0) {  
                if (ret == -1) exit(1);  
                break;  
            }  
            shou[ret] = '\0';  
            strncpy(filename, shou, strlen(filename)+7);  
            printf("接收到的文件名是%s\n",filename);

            //创建并打开文件
            int newFd = open(filename,O_WRONLY|O_CREAT,0666);
            if(-1 == newFd){
                printf("文件创建失败\n");
                exit(-1);
            }
            printf("文件创建成功\n");

            // 循环接收数据并写入文件
            int cnt = 0;
            while (cnt < fileSize) {
                memset(shou, 0, 1024);
                int r = recv(fd, shou, 1024, 0);
                if (r > 0) {
                    write(newFd, shou, r);
                    cnt += r;
                } else if (r <= 0) {
                    perror("recv failed");
                    break;
                }
            }
            printf("文件接收结束,总共接收 %d 字节\n", cnt);
            close(newFd); 
        }else{
            puts(shou);
        }
    }
}

//fa线程用来向服务器发送信息
void* send_info(void* newfd)
{
    int fd = *(int*)newfd;
    char fa[1024];
    while (1)
    {
        memset(fa, 0, 1024);
        scanf("%s", fa);
        if (strcmp(fa, "sendS") == 0){
            printf("客户端准备发送文件\n");

            memset(fa, 0, 1024);
            strcpy(fa,"sendS");
            send(sockfd, fa, strlen(fa), 0);

            sleep(2);

            printf("输入你要发送的文件名: 输入quit退出\n");
            memset(fa, 0, 1024);
            scanf("%s", fa);

            if (strcmp(fa, "quit") == 0) continue;;

            char filname[256];
            memset(filname,0,256);
            strcpy(filname,fa);
            int rfd = open(filname,O_RDONLY);
            if(-1 == rfd){
                printf("请检查文件是否正确\n");
                continue;
            }
            printf("打开文件成功\n");

            send(sockfd, fa, strlen(fa), 0);

            sleep(2);

            //获取文件大小
            int fileSize = lseek(rfd,0,SEEK_END);
            lseek(rfd,0,SEEK_SET);
            int temp = fileSize;
            //发送文件大小
            memset(fa, 0, 1024);
            sprintf(fa, "%d", fileSize);
            send(sockfd, fa, strlen(fa), 0);

            sleep(2);

            //循环读取并发送文件内容
            char buff[256];
            int count = 0;
            while(count < temp){
                memset(buff,0,255);
                int r = read(rfd,buff,255);
                count += r;
                if(r <= 0) break;
                memset(fa, 0, 1024);
                strcpy(fa, buff);
                send(sockfd, fa, strlen(fa), 0);
            }
            printf("文件发送完成\n");
            close(rfd);  

        }else{
            if (send(fd, fa, strlen(fa), 0) == -1)  exit(1);
        }
            
        if (strcmp(fa, "end") == 0)//发送end结束客户端程序
        {
            printf("欢迎使用再见\n");
            close(fd);//关闭套接字
            exit(0);
        }
    }
}
 
int main()
{
    
    int len = sizeof(struct sockaddr);
    struct sockaddr_in otheraddr;
    memset(&otheraddr, 0, len);
    //创建连接
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("sockfd");
        return -1;
    }
    else printf("socket success...%d\n", sockfd);

    otheraddr.sin_family = AF_INET;
    otheraddr.sin_addr.s_addr = inet_addr(IP);
    otheraddr.sin_port = htons(PORT);   
    //连接服务器
    if (connect(sockfd, (struct sockaddr*)&otheraddr, len) == -1){
        perror("connect");
        return -1;
    }
    else{
        printf("connect success\tclient's fd=%d\n", sockfd);
        printf("client ip=%s\tport=%d\n", inet_ntoa(otheraddr.sin_addr), ntohs(otheraddr.sin_port));
    }
    //创建线程
    pthread_t id1, id2;
    char recvbuf[1024];
    char sendbuf[1024];
    memset(recvbuf, 0, 1024);
    memset(sendbuf, 0, 1024);
    //给服务器发送信息，握手判断是否建立连接        
    strcpy(sendbuf, "客户端连接\n");
    if (send(sockfd, sendbuf, strlen(sendbuf), 0) == -1){
        perror("send");
        return -1;
    }
    if (recv(sockfd, recvbuf, 1024, 0) == -1){
        perror("recv");
        return -1;
    }
    else printf("sever say:%s\n", recvbuf);

    //启动客户端线程的收发功能
    pthread_create(&id2, NULL, send_info, &sockfd);
    pthread_create(&id1, NULL, recv_info, &sockfd);
    //等待发送线程结束，退出客户端
    pthread_join(id2, NULL);

    return 0;
}