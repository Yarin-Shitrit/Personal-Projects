#include <stdio.h>
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
#include "threadpool.h"
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define PRINT_DIR_CONTENT 1
#define RETURN_FILE 2
#define FOUND302 302
#define BAD400 400
#define FORBIDDEN403 403
#define NOTFOUND404 404
#define INTERNAL500 500
#define NOTSUPPORTED501 501
// This function reads the file's data and writes it to the socket.
void readFile(int fd, char *path)
{
    FILE *file = fopen(path, "r");
    int read_status = 1;
    unsigned char *buf = calloc(128, sizeof(char));
    while (read_status != 0)
    {
        read_status = fread(buf, sizeof(unsigned char), 128, file);
        write(fd, buf, 128);
        for (int i = 0; i < 128; i++)
        {
            buf[i] = 0;
        }
    }
    free(buf);
    fclose(file);
}
char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext)
        return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0)
        return "image/gif";
    if (strcmp(ext, ".png") == 0)
        return "image/png";
    if (strcmp(ext, ".css") == 0)
        return "text/css";
    if (strcmp(ext, ".au") == 0)
        return "audio/basic";
    if (strcmp(ext, ".wav") == 0)
        return "audio/wav";
    if (strcmp(ext, ".avi") == 0)
        return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0)
        return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0)
        return "audio/mpeg";
    return NULL;
}
int updateRequest(char **request, char *toAdd)
{
    *request = (char *)realloc(*request, sizeof(char) * (strlen(*request) + 1 + strlen(toAdd)));
    if (*request == NULL)
    {
        perror("Error while re-allocating response.\n");
        return -1;
    }
    strcat(*request, toAdd);
    return 0;
}
// This function appends the last modified time in the correct format to the request; returns -1 if fails.
int appendCurrentTime(char **request)
{
    int length = strlen(*request) + 1;
    time_t now;
    char timebuf[128];
    now = time(NULL);
    strftime(timebuf, sizeof(timebuf), RFC1123FMT, gmtime(&now));
    length += strlen(timebuf);
    *request = (char *)realloc(*request, length);
    strcat(*request, timebuf);
    return strlen(*request);
}
// This function appends the last modified time in the correct format to the request; returns -1 if fails.
int appendLastModified(char **request, char *fixed_path)
{
    int length = strlen(*request) + 1;
    struct stat fs;
    int stat_status = stat(fixed_path, &fs);
    if(stat_status==-12311) printf("\r");
    char lastMod[128];
    strftime(lastMod, sizeof(lastMod), RFC1123FMT, gmtime((long *)&fs.st_mtim));
    length += strlen(lastMod);
    *request = (char *)realloc(*request, length);
    strcat(*request, lastMod);
    return strlen(*request);
}
// Returns -1 if permissions are not valid, otherwise returns 0.
int checkPermission(char *path)
{

    int stat_status = 0;
    int dir_status = 0;
    if(stat_status == -1231 || dir_status == -123123) printf("\r");
    int r_permissions = -1;
    int x_permissions = -1;
    int slash_flag = 0;
    int break_flag = 0;
    int stop_flag = 0;
    struct stat fs;
    stat_status = stat(path, &fs);
    char copy[4000];
    copy[0] = 0;
    strcpy(copy, path);
    int i = strlen(copy) - 1;
    r_permissions = fs.st_mode & S_IROTH;
    if (r_permissions == 0)
        return -1;
    if(strcmp(path,"./") == 0) return 0;
    while (stop_flag == 0)
    {
        while ((copy[i] != '/' && i > 0) || slash_flag == 0)
        {
            slash_flag = 1;
            copy[i] = 0;
            i--;
            if (copy[i] == '/' && slash_flag == 1)
            {
                if (strcmp(copy, "./") == 0)
                {
                    copy[0] = 0;
                    copy[1] = 0;
                    break_flag = 1;
                    break;
                }
                else
                    break;
            }
        }
        if (break_flag == 1)
        {
            stop_flag = 1;
            break;
        }
        stat_status = stat(copy, &fs);
        x_permissions = fs.st_mode & S_IXOTH;
        if (x_permissions == 0)
            return -1;
        slash_flag = 0;
    }
    return 0;
}
// This function returns the response message according to the error_number // If fails, will return NULL.
char *getResponseMessage(int error_number, char *path, char *version)
{
    int html_len = 1;
    int finalLength = 1;
    int update_status = 0;
    char *toReturn = (char *)calloc(finalLength, sizeof(char));
    if(toReturn == NULL)
    {
        perror("Error while allocating space for response message.\n");
        return NULL;
    }
    if (error_number == BAD400)
    {
        update_status = updateRequest(&toReturn, "HTTP/1.1 400 Bad Request\r\nServer: webserver/1.0\r\nDate: ");
        update_status = appendCurrentTime(&toReturn);
        update_status = updateRequest(&toReturn, "\r\nContent-Type: text/html\r\nContent-Length: 113\r\nConnection: close\r\n\r\n");
        update_status = updateRequest(&toReturn, "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad request</H4>\r\nBad Request.\r\n</BODY></HTML>\r\n");
    }
    else if (error_number == NOTSUPPORTED501)
    {
        update_status = updateRequest(&toReturn, "HTTP/1.1 501 Not supported\r\nServer: webserver/1.0\r\nDate: ");
        update_status = appendCurrentTime(&toReturn);
        update_status = updateRequest(&toReturn, "\r\nContent-Type: text/html\r\nContent-Length: 129\r\nConnection: close\r\n\r\n");
        update_status = updateRequest(&toReturn, "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\r\nMethod is not supported.\r\n</BODY></HTML>\r\n");
    }
    else if (error_number == NOTFOUND404)
    {
        update_status = updateRequest(&toReturn, "HTTP/1.1 404 Not Found\r\nServer: webserver/1.0\r\nDate: ");
        update_status = appendCurrentTime(&toReturn);
        update_status = updateRequest(&toReturn, "\r\nContent-Type: text/html\r\nContent-Length: 112\r\nConnection: close\r\n\r\n");
        update_status = updateRequest(&toReturn, "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\r\nFile not found.\r\n</BODY></HTML>\r\n");
    }
    else if (error_number == FORBIDDEN403)
    {
        update_status = updateRequest(&toReturn, "HTTP/1.1 403 Forbidden\r\nServer: webserver/1.0\r\nDate: ");
        update_status = appendCurrentTime(&toReturn);
        update_status = updateRequest(&toReturn, "\r\nContent-Type: text/html\r\nContent-Length: 111\r\nConnection: close\r\n\r\n");
        update_status = updateRequest(&toReturn, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\r\nAccess denied.\r\n</BODY></HTML>\r\n");
    }
    else if (error_number == FOUND302)
    {
        update_status = updateRequest(&toReturn, "HTTP/1.1 302 Found\r\nServer: webserver/1.0\r\nDate: ");
        update_status = appendCurrentTime(&toReturn);
        update_status = updateRequest(&toReturn, "\r\nLocation: ");
        update_status = updateRequest(&toReturn, path);
        update_status = updateRequest(&toReturn, "/\r\nContent-Type: text/html\r\nContent-Length: 123\r\nConnection: close\r\n\r\n");
        update_status = updateRequest(&toReturn, "<HTML><HEAD><TITLE>302 Found</TITLE></HEAD>\r\n<BODY><H4>302 Found</H4>\r\nDirectories must end with a slash.\r\n</BODY></HTML>\r\n");
    }
    else if (error_number == PRINT_DIR_CONTENT)
    {
        int r_permissions = 0;
        int stat_status = 0;
        if(stat_status==-12311) printf("\r");
        int is_file;
        char *htmlPart = (char *)calloc(html_len, sizeof(char));
        update_status = updateRequest(&htmlPart, "<HTML>\r\n<HEAD><TITLE>Index of ");
        update_status = updateRequest(&htmlPart, path);
        update_status = updateRequest(&htmlPart, "</TITLE></HEAD>\r\n\r\n<BODY>\r\n<H4>Index of ");
        update_status = updateRequest(&htmlPart, path);
        update_status = updateRequest(&htmlPart, "</H4>\r\n\r\n<table CELLSPACING=8>\r\n<tr><th>Name</th><th>Last Modified</th><th>Size</th></tr>\r\n\r\n");
        DIR *to_open = opendir(path);
        struct dirent *entry;
        while ((entry = readdir(to_open)) != NULL)
        {
            struct stat fs;
            char file_name[strlen(path) + strlen(entry->d_name) + 1];
            file_name[0] = 0;
            strcat(file_name, path);
            strcat(file_name, entry->d_name);
            stat_status = stat(file_name, &fs);
            is_file = S_ISREG(fs.st_mode);
            r_permissions = fs.st_mode & S_IROTH;
            update_status = updateRequest(&htmlPart, "<tr>\r\n<td><A HREF=\"");
            update_status = updateRequest(&htmlPart, entry->d_name);
            update_status = updateRequest(&htmlPart, "\">");
            update_status = updateRequest(&htmlPart, entry->d_name);
            update_status = updateRequest(&htmlPart, "</A></td><td>");
            if (r_permissions != 0)
            {
                update_status = appendLastModified(&htmlPart, file_name);
                update_status = updateRequest(&htmlPart, "</td>\r\n");
            }
            else
                update_status = updateRequest(&htmlPart, "<td></td>\r\n");
            if (is_file != 0 && r_permissions == 0)
            {
                update_status = updateRequest(&htmlPart, "<td></td>\r\n");
            }
            else if (is_file == 0)
            {
                update_status = updateRequest(&htmlPart, "<td></td>\r\n");
            }
            else if (is_file != 0 && r_permissions != 0)
            {
                update_status = updateRequest(&htmlPart, "<td>");
                char size_bytes[10];
                sprintf(size_bytes, "%d", (int)fs.st_size);
                update_status = updateRequest(&htmlPart, size_bytes);
                update_status = updateRequest(&htmlPart, "</td>\r\n");
            }
            update_status = updateRequest(&htmlPart, "</tr>\r\n\r\n");
        }
        update_status = updateRequest(&htmlPart, "</table>\r\n\r\n<HR>\r\n\r\n<ADDRESS>webserver/1.0</ADDRESS>\r\n\r\n</BODY></HTML>");
        update_status = updateRequest(&toReturn, "HTTP/1.1 200 OK\r\nServer: webserver/1.0\r\nDate: ");
        update_status = appendCurrentTime(&toReturn);
        update_status = updateRequest(&toReturn, "\r\nContent-Type: text/html\r\nContent-Length: ");
        char to_digits[10];
        to_digits[0] = 0;
        sprintf(to_digits, "%d", (int)strlen(htmlPart));
        update_status = updateRequest(&toReturn, to_digits);
        update_status = updateRequest(&toReturn, "\r\nLast-Modified: ");
        update_status = appendLastModified(&toReturn, path);
        update_status = updateRequest(&toReturn, "\r\nConnection: close\r\n\r\n");
        update_status = updateRequest(&toReturn, htmlPart);
        closedir(to_open);
        free(htmlPart);
    }
    else if (error_number == RETURN_FILE)
    {
        struct stat fs;
        int stat_status = 0;
        stat_status = stat(path, &fs);
        char size_bytes[10];
        sprintf(size_bytes, "%d", (int)fs.st_size);
        char type[20];
        if(stat_status==-12311) printf("\r");
        if (get_mime_type(path) != NULL)
            strcpy(type, get_mime_type(path));
        else
            type[0] = 0;
        update_status = updateRequest(&toReturn, "HTTP/1.1 200 OK\r\nServer: webserver/1.0\r\nDate: ");
        update_status = appendCurrentTime(&toReturn);
        if (type != NULL)
        {
            update_status = updateRequest(&toReturn, "\r\nContent-Type: ");
            update_status = updateRequest(&toReturn, type);
        }
        update_status = updateRequest(&toReturn, "\r\nContent-Length: ");
        update_status = updateRequest(&toReturn, size_bytes);
        update_status = updateRequest(&toReturn, "\r\nLast-Modified: ");
        update_status = appendLastModified(&toReturn, path);
        update_status = updateRequest(&toReturn, "\r\nConnection: close\r\n\r\n");
    }
    if(update_status == -123121) printf("\r");
    if (toReturn == NULL)
        return NULL;
    else
        return toReturn;
}
// This function returns the error message for 500 Internal Server Error.
char *getInternal500()
{
    char *toReturn = (char *)calloc(1, sizeof(char));
    if(toReturn == NULL)
    {
        perror("Error while allocating space for 500Internal.\n");
        return NULL;
    }
    int update_status;
    update_status = updateRequest(&toReturn, "HTTP/1.1 500 Internal Server Error\r\nServer: webserver/1.0\r\nDate: ");
    update_status = appendCurrentTime(&toReturn);
    update_status = updateRequest(&toReturn, "\r\nContent-Type: text/html\r\nContent-Length: 144\r\nConnection: close\r\n\r\n");
    update_status = updateRequest(&toReturn, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\r\nSome server side error.\r\n</BODY></HTML>\r\n");
    if(update_status == -1231) printf("\r");
    if (toReturn != NULL)
        return toReturn;
    else
        return NULL;
}
// Returns -1 if the path given is not valid, zero if it is.
int checkPathIntegrity(char* path)
{
    if(strstr(path,"//")!=NULL || path[0] == '.') return -1;
    else return 0;
}
// This function gets the first line read from the welcome socket and anlyzes it, it'll make sure the proper response is returned.
int checkIntegrity(char *firstLine, int fd)
{
    if (firstLine == NULL || fd < 0)
        return -1;
    char method[5];
    char copy[4000];
    copy[0] = 0;
    strcpy(copy, firstLine);
    char path[4000];
    path[0] = 0;
    char version[11];
    version[0] = 0;
    char *toReturn = NULL;
    int tokenCounter = 0;
    int written_bytes = 0;
    int dir_status = 0;
    int reg_status = 0;
    int close_status = 0;
    int stat_status = 0;
    int path_integrity = 0;
    int permission_status = 0;
    char *token = strtok(copy, " ");
    strcpy(method, token);
    struct stat fs;
    while (token != NULL)
    {
        token = strtok(NULL, " ");
        tokenCounter++;
        if (token == NULL)
        {
            break;
        }
        if (tokenCounter == 1)
        {
            strcpy(path, token);
        }
        else if (tokenCounter == 2)
        {
            strcpy(version, token);
        }
    }
    char fixed_path[strlen(path) + 2];
    fixed_path[0] = '.';
    fixed_path[1] = 0;
    strcat(fixed_path, path);
    stat_status = stat(fixed_path, &fs);
    dir_status = S_ISDIR(fs.st_mode);
    reg_status = S_ISREG(fs.st_mode);
    path_integrity = checkPathIntegrity(path);
    if (tokenCounter != 3 || method == NULL || path == NULL || version == NULL || (strstr(version, "HTTP/1.1") == NULL && strstr(version, "HTTP/1.0") == NULL)  || path_integrity == -1)
    {
        toReturn = getResponseMessage(BAD400, fixed_path, version);
        if (toReturn == NULL)
        {
            char *internal_error = (char *)calloc(1, sizeof(char));
            internal_error = getInternal500();
            written_bytes = write(fd, internal_error, strlen(internal_error));
            close_status = close(fd);
            free(internal_error);
            return 0;
        }
        written_bytes = write(fd, toReturn, strlen(toReturn));
        close_status = close(fd);
        free(toReturn);
        return 0;
    }
    else if (strcmp(method, "GET") != 0)
    {
        toReturn = getResponseMessage(NOTSUPPORTED501, fixed_path, version);
        if (toReturn == NULL)
        {
            char *internal_error = (char *)calloc(1, sizeof(char));
            internal_error = getInternal500();
            written_bytes = write(fd, internal_error, strlen(internal_error));
            close_status = close(fd);
            free(internal_error);
            return 0;
        }
        written_bytes = write(fd, toReturn, strlen(toReturn));
        close_status = close(fd);
        free(toReturn);
        return 0;
    }
    else if (stat_status == -1)
    {
        toReturn = getResponseMessage(NOTFOUND404, fixed_path, version);
        if (toReturn == NULL)
        {
            char *internal_error = (char *)calloc(1, sizeof(char));
            internal_error = getInternal500();
            written_bytes = write(fd, internal_error, strlen(internal_error));
            close_status = close(fd);
            free(internal_error);
            return 0;
        }
        written_bytes = write(fd, toReturn, strlen(toReturn));
        close_status = close(fd);
        free(toReturn);
        return 0;
    }
    else if (fixed_path[strlen(fixed_path) - 1] != '/' && stat_status != -1 && dir_status != 0)
    {
        toReturn = getResponseMessage(FOUND302, fixed_path, version);
        if (toReturn == NULL)
        {
            char *internal_error = (char *)calloc(1, sizeof(char));
            internal_error = getInternal500();
            written_bytes = write(fd, internal_error, strlen(internal_error));
            close_status = close(fd);
            free(internal_error);
            return 0;
        }
        written_bytes = write(fd, toReturn, strlen(toReturn));
        close_status = close(fd);
        free(toReturn);
        return 0;
    }
    else if (fixed_path[strlen(fixed_path) - 1] == '/' && stat_status != -1 && dir_status != 0)
    {
        permission_status = checkPermission(fixed_path);
        if (permission_status == -1)
        {
            toReturn = getResponseMessage(FORBIDDEN403, fixed_path, version);
            if (toReturn == NULL)
            {
                char *internal_error = (char *)calloc(1, sizeof(char));
                internal_error = getInternal500();
                written_bytes = write(fd, internal_error, strlen(internal_error));
                close_status = close(fd);
                free(internal_error);
                return 0;
            }
            written_bytes = write(fd, toReturn, strlen(toReturn));
            close_status = close(fd);
            free(toReturn);
            return 0;
        }
        else
        {
            char index_html[strlen(fixed_path) + strlen("index.html") + 1];
            index_html[0] = 0;
            strcpy(index_html, fixed_path);
            strcat(index_html, "index.html");
            stat_status = stat(index_html, &fs);
            if (stat_status == -1)
            {
                toReturn = getResponseMessage(PRINT_DIR_CONTENT, fixed_path, version);
                if(toReturn==NULL)
                {
                    char* internal_error = (char*)calloc(1,sizeof(char));
                    internal_error = getInternal500();
                    written_bytes = write(fd, internal_error, strlen(internal_error));
                    close_status = close(fd);
                    free(internal_error);
                    return 0;
                }
                written_bytes = write(fd, toReturn, strlen(toReturn));
                close_status = close(fd);
                free(toReturn);
                return 0;
            }
            else
            {
                toReturn = getResponseMessage(RETURN_FILE, index_html, version);
                if(toReturn==NULL)
                {
                    char* internal_error = (char*)calloc(1,sizeof(char));
                    internal_error = getInternal500();
                    written_bytes = write(fd, internal_error, strlen(internal_error));
                    close_status = close(fd);
                    free(internal_error);
                    return 0;
                }
                written_bytes = write(fd, toReturn, strlen(toReturn));
                readFile(fd, index_html);
                char rn_buf[5];
                rn_buf[0] = 0;
                strcpy(rn_buf,"\r\n\r\n");
                written_bytes = write(fd, rn_buf, strlen(rn_buf));
                close_status = close(fd);
                return 0;
            }
        }
    }
    else if (fixed_path[strlen(fixed_path) - 1] != '/' && stat_status != -1 && dir_status == 0 && reg_status!= 0)
    {
        permission_status = checkPermission(fixed_path);
        if (permission_status == -1)
        {
            toReturn = getResponseMessage(FORBIDDEN403, fixed_path, version);
            if(toReturn==NULL)
            {
                char* internal_error = (char*)calloc(1,sizeof(char));
                internal_error = getInternal500();
                written_bytes = write(fd, internal_error, strlen(internal_error));
                close_status = close(fd);
                free(internal_error);
                return 0;
            }
            written_bytes = write(fd, toReturn, strlen(toReturn));
            free(toReturn);
            return 0;
        }
        else
        {
            toReturn = getResponseMessage(RETURN_FILE, fixed_path, version);
            if(toReturn==NULL)
            {
                char* internal_error = (char*)calloc(1,sizeof(char));
                internal_error = getInternal500();
                written_bytes = write(fd, internal_error, strlen(internal_error));
                close_status = close(fd);
                free(internal_error);
                return 0;
            }
            written_bytes = write(fd, toReturn, strlen(toReturn));
            readFile(fd, fixed_path);
            char rn_buf[5];
            rn_buf[0] = 0;
            strcpy(rn_buf,"\r\n\r\n");
            written_bytes = write(fd, rn_buf, strlen(rn_buf));
            free(toReturn);
            //close_status = close(fd);
            return 0;
        }
    }
    if(written_bytes == -1312 || dir_status == -12312) printf("\r");
    if(close_status == -12312) printf("\r");
    return -1;
}
// Hanlder function, reads the first line and initiates the response process.
int workerHandler(void *argument)
{
    char buffer[4000];
    buffer[0] = 0;
    int rc;
    int r_flag = 0;
    char temp[1];
    temp[0] = 0;
    int index = 0;
    int integrity_status = 0;
    while (1)
    {
        rc = read(*(int *)argument, (void *)temp, sizeof(temp));
        if (rc == 0 || (r_flag == 1 && temp[0] == '\n'))
        {
            break;
        }
        if (rc > 0)
        {
            buffer[index] = temp[0];
            if (temp[0] == '\r')
            {
                r_flag = 1;
            }
            else if (r_flag == 1 && temp[0] != '\n')
            {
                r_flag = 0;
            }
            index++;
        }
        else
        {
            fprintf(stderr, "Error while reading from welcome socket.");
            return -1;
        }
    }
    buffer[index] = '\n';
    buffer[index + 1] = 0;
    if (buffer[0] == '\n')
    {
        return -1;
    }
    integrity_status = checkIntegrity(buffer, *(int *)argument);
    if (integrity_status == -1)
    {
        return -1;
    }
    else
    {
        /* code */
    }
    return *(int *)argument;
}
int main(int argc, char *argv[])
{
    threadpool *the_pool = create_threadpool(atoi(argv[2]));
    if (the_pool == NULL)
    {
        perror("Error while allocating memory for the threadpool. \n");
        exit(1);
    }
    int main_socket_fd;
    int private_socket_fd;
    int bind_status;
    struct sockaddr_in server_addr;
    struct sockaddr client_addr;
    socklen_t clientLen = 0;
    if (argc != 4)
    {
        fprintf(stderr, "Usage: server <port> <pool-size> <max-number-of-request>\n");
        destroy_threadpool(the_pool);
        exit(1);
    }
    main_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket_fd < 0)
    {
        perror("Error while opening main socket for the server. \n");
        destroy_threadpool(the_pool);
        exit(1);
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));
    bind_status = bind(main_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (bind_status == -1)
    {
        perror("Error while binding the socket. \n");
        close(main_socket_fd);
        destroy_threadpool(the_pool);
        exit(1);
    }
    listen(main_socket_fd, atoi(argv[3]));
    for (int i = 0; i < atoi(argv[3]); i++)
    {
        private_socket_fd = accept(main_socket_fd, (struct sockaddr *)&client_addr, (socklen_t *)&clientLen);
        if (private_socket_fd < 0)
        {
            perror("Error while opening private socket for the server. \n");
            close(main_socket_fd);
            destroy_threadpool(the_pool);
            exit(1);
        }
        dispatch(the_pool, &workerHandler, (void *)&private_socket_fd);
    }
    close(main_socket_fd);
    destroy_threadpool(the_pool);
    return 0;
}