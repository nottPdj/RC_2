/**      (C)2000-2021 FEUP
 *       tidy up some includes and parameters
 * */

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>

#include <string.h>

#define BUF_SIZE 512

int getIP(char* hostname, char* ip) {
    struct hostent *h;

    #define h_addr h_addr_list[0]

    if ((h = gethostbyname(hostname)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }
    strncpy(ip, inet_ntoa(*((struct in_addr *) h->h_addr)), 17);
    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", ip);
    return 0;
}

int createSocket(char* ip, int port){
    int sockfd;
    struct sockaddr_in server_addr;
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    return sockfd;
}

int readResponse(int sockfd, char *buf, int buf_size, const char* expected_codes[], int num_codes) {
    memset(buf, 0, buf_size); 
    int total_bytes = 0, bytes = 0;
    char line[BUF_SIZE] = {0};
    int matched = 0; 
    int errorCode = 1;

    while ((bytes = read(sockfd, line, BUF_SIZE - 1)) > 0) {
        line[bytes] = '\0';  
        strcat(buf, line);  
        total_bytes += bytes;

        for (int i = 0; i < num_codes; i++) {
            char code_prefix[16];
            snprintf(code_prefix, sizeof(code_prefix), "%s ", expected_codes[i]);  
            //printf("CODE PREFIX:%s\n",code_prefix);
            if (strstr(line, code_prefix) != NULL) {
                matched = 1;
                errorCode = 0;  
                break; 
            }

            snprintf(code_prefix, sizeof(code_prefix), "%s-", expected_codes[i]);
            if (strstr(line, code_prefix) != NULL) {
                matched = 0;
                errorCode = 0;
                break;
            }
        }

        if(errorCode){
          printf("%s",line);
          exit(-1);  
        }

        if (matched) {
            break;  
        }
        memset(line, 0, BUF_SIZE);  
    }
    if (!matched) {
        perror("code()");
        exit(-1);  
    }
    
    if (bytes < 0) {
        perror("read()");
    }

    return total_bytes; 
}



int main(int argc, char **argv) {
    char* url = argv[1];
    char user[256] = {0}, pass[256] = {0}, hostname[256] = {0}, path[256] = {0};
    char ip[17] = {0};
    int bytes = 0;
    char buf[BUF_SIZE] = {0};

    if (sscanf(url, "ftp://%255[^:@]:%255[^@]@%255[^/]/%s", user, pass, hostname, path) == 4) {
    }
    else {
        if (sscanf(url, "ftp://%255[^/]/%s", hostname, path) == 2) {
            strcpy(user, "anonymous");
            strcpy(pass, "anonymous");
            printf("User: %s\nPass: %s\nHost: %s\nPath: %s\n", user, pass, hostname, path);
        }
    }

    getIP(hostname, ip);
    int sockfd = createSocket(ip, 21);
    usleep(100000);     //VERIFICAR ESTE SLEEP
    bytes=read(sockfd,buf, BUF_SIZE);
    printf("%s\n",buf);
    memset(buf,0,BUF_SIZE);

    const char *user_expected[] = { "331", "230" };
    sprintf(buf, "USER %s\r\n", user);

    bytes = write(sockfd, buf, strlen(buf));
    if (bytes > 0){
        printf("%s",buf);
        memset(buf, 0, BUF_SIZE);
        readResponse(sockfd, buf, BUF_SIZE, user_expected,2);
        printf("%s",buf);
    }
    else {
        perror("write()");
        exit(-1);
    }

    const char *pass_expected[] = { "230", "202" };
    sprintf(buf, "PASS %s\r\n", pass);
    bytes = write(sockfd, buf, strlen(buf));
    if (bytes > 0){
        printf("%s",buf);
        memset(buf, 0, BUF_SIZE);
        readResponse(sockfd, buf, BUF_SIZE, pass_expected,2);
        printf("%s",buf);
    }
    else {
        perror("write()");
        exit(-1);
    }

    const char *pasv_expected[] = { "227" };
    char pasv[7] = "pasv\r\n";
    int ip1, ip2, ip3, ip4;
    int port1, port2;
    char ipPasv[256];
    int portPasv = 0;
    
    bytes = write(sockfd, pasv, strlen(pasv));
    if (bytes > 0){
        printf("%s",pasv);
        memset(buf, 0, BUF_SIZE);
        readResponse(sockfd, buf, BUF_SIZE, pasv_expected,1);
        sscanf(buf, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d).", &ip1, &ip2, &ip3, &ip4, &port1, &port2);
        printf("%s",buf);
    }
    else {
        perror("write()");
        exit(-1);
    }

    sprintf(ipPasv, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    portPasv = 256 * port1 + port2;
    
    int sockfdpasv = createSocket(ipPasv, portPasv);
    memset(buf, 0, BUF_SIZE);
    
    const char *retr_expected[] = { "150", "125" };
    sprintf(buf, "retr %s\r\n", path);

    bytes = write(sockfd, buf, strlen(buf));
    int file_sz = 0;
    if (bytes > 0){
        printf("%s",buf);
        memset(buf, 0, BUF_SIZE);
        readResponse(sockfd, buf, BUF_SIZE, retr_expected, 2);
        //sscanf(buf, "150 Opening BINARY mode data connection for %s (%d bytes).", path ,&file_sz);
        printf("%s",buf);
    }
    else {
        perror("write()");
        exit(-1);
    }
    char file_downloaded[256];
    const char *last_slash = strrchr(path, '/');
    if (last_slash && *(last_slash + 1) != '\0') {
        strcpy(file_downloaded, last_slash + 1);
    } else {
        strcpy(file_downloaded, path);
    }

    FILE *filefd = fopen(file_downloaded,"wb");
    if (!filefd) {
        perror("Error opening file for writing");
        close(sockfdpasv);
        exit(-1);
    }
    int file_bytes =0;
    while ((file_bytes = read(sockfdpasv, buf, BUF_SIZE)) > 0) {
        int written = fwrite(buf, 1, file_bytes, filefd);
        if (written != file_bytes) {
            perror("Error writing to file");
            fclose(filefd);
            close(sockfdpasv);  
            exit(-1);
        }
    }

    if (file_bytes < 0) {
        perror("Error reading from data connection");
    }
    
    fclose(filefd);

    if (close(sockfdpasv)<0) {
        perror("close()");
        exit(-1);
    }

    const char *transfer_completed[] = { "226"}; 
    readResponse(sockfd, buf, BUF_SIZE, transfer_completed,1);
    printf("%s",buf);
   
    const char *quit_expected[] = { "221" };
    sprintf(buf, "QUIT\r\n");
    bytes = write(sockfd, buf, strlen(buf));
    if (bytes > 0){
        printf("%s",buf);
        memset(buf, 0, BUF_SIZE);
        readResponse(sockfd, buf, BUF_SIZE, quit_expected,1);
        printf("%s",buf);
    }

    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }
    return 0;
}


