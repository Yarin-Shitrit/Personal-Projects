#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
static int main_socket_fd;
int isNumber(char *str) // Checks if the given string is a numerical string, returns 0 if the string is a numerical string, else -1.
{
    int i = 0;
    for (i = 0; i < strlen(str); i++) 
    {
        if (isdigit(str[i]) == 0) 
        {
            return -1;
        }
    }
    return 0;
}
typedef struct message // A linked list struct to maintain the messages.
{
    int fd;
    char* msg_content;
    struct message *next_msg;
} message;
void freeHandler() // Handler for SIGINT.
{
    close(main_socket_fd);
    exit(0);
}
int main(int argc, char *argv[])
{
    signal(SIGINT, freeHandler);
    //--Variables initialization--//
    int private_socket_fd = 0;
    int bind_status = 0;
    int listen_status = 0;
    int read_status = 0;
    int write_status = 0;
    int private_index = 0;
    //--Buffers initialization--//
    char buff[4096];
    char guest[30];
    char guest_num[7];
    int private_sockets[1000];
    private_sockets[0] = 0;
    bzero(buff, 4096);
    bzero(private_sockets, 1000);
    bzero(guest_num, 7);
    bzero(guest, 30);
    //--Server Code--//
    struct sockaddr_in server_addr;
    fd_set main_set;
    fd_set copy_read;
    int max = 0;
    fd_set copy_write;
    message *msg = NULL;
    message *first_msg = NULL;
    message *last_msg = NULL;
    message *pulled_msg = NULL;
    int messages_num = 0;
    main_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    max = 1 + main_socket_fd;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));
    bind_status = bind(main_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if(argc != 2) // If there are too many or less arguments than needed.
    {
        fprintf(stderr,"Usage: server <port> \n");
        close(main_socket_fd);
        exit(1);
    }
    if(isNumber(argv[1])==-1) // If port argument is not a numerical or a negative port.
    {
        fprintf(stderr,"Usage: server <port> \n");
        close(main_socket_fd);
        exit(1);
    }
    if (bind_status == -1)
    {
        perror("Error while binding the socket. \n");
        close(main_socket_fd);
        exit(1);
    }
    listen_status = listen(main_socket_fd, 5);
    if (listen_status == -1)
    {
        perror("Error while listening to the socket. \n");
        close(main_socket_fd);
        exit(1);
    }
    FD_ZERO(&main_set);
    FD_SET(main_socket_fd, &main_set);
    while (1)
    {
        copy_write = main_set;
        copy_read = main_set;
        bzero(buff, 4096);
        select(max, &copy_read, &copy_write, NULL, NULL);
        if (FD_ISSET(main_socket_fd, &copy_read))
        {
            private_socket_fd = accept(main_socket_fd, NULL, NULL);
            if (private_socket_fd < 0)
            {
                perror("Error while listening to the socket. \n");
                close(main_socket_fd);
                exit(1);
            }
            if (private_socket_fd >= max)
                max = private_socket_fd + 1;
            FD_SET(private_socket_fd, &main_set);
            private_sockets[private_index] = private_socket_fd;
            private_index++;
        }
        for (int i = 0 ; i < private_index; i++)
        {
            if (FD_ISSET(private_sockets[i], &copy_read))
            {
                
                read_status = read(private_sockets[i], buff, 4096);
                printf("Server is ready to read from private-socket number: %d\n", private_socket_fd);
                if (read_status == 0)
                {
                    FD_CLR(private_sockets[i], &main_set);
                    close(private_sockets[i]);
                    private_sockets[i] = -1;
                    
                }
                else if (read_status > 0)
                {
                    if (messages_num == 0)
                    {
                        msg = (message *)calloc(1,sizeof(message));
                        msg->fd = private_sockets[i];
                        msg->next_msg = NULL;
                        msg->msg_content = (char*)calloc(1+strlen(buff),sizeof(char));
                        strcpy(msg->msg_content, buff);
                        first_msg = msg;
                        last_msg = msg;
                        messages_num++;
                    }
                    else
                    {
                        msg = (message *)calloc(1,sizeof(message));
                        msg->fd = private_sockets[i];
                        msg->next_msg = last_msg->next_msg;
                        msg->msg_content = (char*)calloc(1+strlen(buff),sizeof(char));
                        strcpy(msg->msg_content, buff);
                        last_msg->next_msg = msg;
                        last_msg = msg;
                        messages_num++;
                    }
                }
            }
        }
        if (messages_num > 0)
        {
            for (int i = 0; i < private_index; i++)
            {
                pulled_msg = first_msg;
                if (private_sockets[i] != pulled_msg->fd && private_sockets[i] != -1)
                {
                    printf("Server is ready to write to socket: %d\n", private_sockets[i]);
                    sprintf(guest_num, "%d", pulled_msg->fd);
                    bzero(guest,30);
                    strcpy(guest, "guest");
                    strcat(guest, guest_num);
                    strcat(guest, ": ");
                    write_status = write(private_sockets[i], guest, strlen(guest));
                    write_status = write(private_sockets[i], pulled_msg->msg_content, strlen(pulled_msg->msg_content));
                    if (write_status < 0)
                    {
                        perror("Error while writing to the socket.\n");
                        close(main_socket_fd);
                        exit(1);
                    }
                }
                else
                    continue;
            }
            if (pulled_msg != NULL)
            {
                first_msg = pulled_msg->next_msg;
                free(pulled_msg->msg_content);
                free(pulled_msg);
                messages_num--;
                
            }
        }
        
    }
    close(main_socket_fd);
    return 0;
}