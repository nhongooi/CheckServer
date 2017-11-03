#include "threadPool.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>
// Global define value
#define DEF_PORT "8080" // default port
#define MAX_THREAD 10
#define BUFF_SIZE 128
#define MAX_QUEUE 10
#define MAX_CLIENT 100
#define DEF_DICT "DEFAULT_DICTIONARY"

// global data struct
struct dictionary *main_dict; // global dictionary
struct threadpool *tpool;
//funtions
void print_peerip(int socketfd); // get peer ip and print
void print_hostip();// print all host ips
int server_start(struct threadpool *pool, char *port);//
struct dictionary *Dictionary_init(char *dict_name);//
char *remove_n(char *word, int word_len);

// input order: [dictionary] [port]
int main(int argc, char **argv){
    char *user_port;
    int status;
    int compInt;
    //init threadpool
    tpool = threadpool_init(MAX_THREAD, MAX_QUEUE);
    //check for dictionary name
    //init dictionary
    main_dict = Dictionary_init(argv[1]);

    //check for correct port 
    if(argv[2] != NULL)
        compInt = atoi(argv[2]);
    if((compInt > 1023) && (compInt < 65535))
        user_port = argv[2];
    else{
        printf("port not in range\n");
        user_port = DEF_PORT;
    }
    printf("Port: %s\n", user_port);
    //start server
    status = server_start(tpool, user_port);
    return EXIT_SUCCESS;
}
//how server init socket and starts
int server_start(struct threadpool *pool, char *port){
    //init sockets and sattuses
    int listenfd, connfd;
    struct sockaddr_in serv_sock;
    int opt = 1;
    int socklen = sizeof(serv_sock);
    int status;
      
    bzero(&serv_sock, sizeof(serv_sock));
    // Creating socket file descriptor
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        raise_error("init Socket");
      
    // Forcefully attaching socket to the port
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,&opt, sizeof(opt)) < 0)
        raise_error("socket setsockopt");
    
    //set up socket
    serv_sock.sin_family = AF_INET;
    serv_sock.sin_addr.s_addr = INADDR_ANY;
    serv_sock.sin_port = htons(atoi(port));
      
    if (bind(listenfd, (struct sockaddr *)&serv_sock,sizeof(serv_sock))<0) 
        raise_error("socket bind");

    //get list of host ip
    printf("*Print all listening IP*\n");
    print_hostip();
    
    if (listen(listenfd, MAX_CLIENT) < 0) raise_error("socket listen");

    while(1){
        struct sockaddr_in client_sock;
        int client_len = sizeof(client_sock);
        printf("accepting\n");        
        if ((connfd = accept(listenfd, (struct sockaddr *)&client_sock, (socklen_t*) &client_len)) < 0)
            raise_error("socket accept");
        //wait til the queue is free
        
        //busy waiting for queue to be empty
        do{
            //returns 1 if full, -1 for error and 0 for EXIT_SUCESS        
            if((status = threadpool_add(pool, connfd)) < 0) fprintf(stderr, "Error adding client to queue\n");
        }while(status != 0);   
        //print peer ip
        print_peerip(connfd);
    }
    // shutdown all sockets
    if(shutdown(listenfd, 2) < 0) raise_error("closing listenfd");
    if(shutdown(connfd, 2) <0) raise_error("closing connfd");
    return EXIT_SUCCESS;
}
// checking each word through library and send back reposnse through socket
int wordChecking(int socket){
    char *buf = malloc(sizeof(char) * BUFF_SIZE); // buffering for sent char
    int iscorrect = 0;
    char *ok = "-OK\n";
    char *miss = "-MISSPELLED\n";
    int ok_size = strlen(ok);
    int miss_size = strlen(miss);
    int i;
    int len;
    int recv_size;
    check_ptr(buf);

    while((recv_size = recv(socket,buf, sizeof(char)* BUFF_SIZE,0)) > 1){
        //get message from socket to buf and size of by bytes to recv_size
        buf = remove_n(buf, recv_size);
        len = strlen(buf);
        //loop through dictionary
        for(i = 0; i < main_dict->entries; i++){
            //if found a match in dictionary, correct spelling
            if((strcmp(buf,main_dict->word_list[i])) == 0){
                // remove \n to strcat
                buf[len-1] = '\0';
                len = strlen(buf);
                // cat OK
                strncat(buf, ok, len+ok_size);
                if(send(socket, buf, strlen(buf),0) <0) raise_error("correct send");
                iscorrect = 1;
                break;
            }
        }
        // if there was no correct spelling
        if(iscorrect == 0){
        //remove \n to strcat
          buf[len-1] = '\0';
          len = strlen(buf);
          strncat(buf, miss, len+miss_size);
          if(send(socket, buf, strlen(buf),0) <0) raise_error("correct send"); 
        }
        //gotta reset this for next word
        iscorrect = 0;
        free(buf);
        buf = malloc(sizeof(char) *BUFF_SIZE);
        check_ptr(buf);
    }
    free(buf);
    // end comm and close socket  
    if(shutdown(socket, 2) < 0) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}
//load dictionary into memory
struct dictionary *Dictionary_init(char *dict_name){
    FILE *fd; // file descriptor
    int len = 0; // number of entries
    int pos = 0; // position for library
    //for readline
    char *buffer = NULL; 
    ssize_t buf_size = 0;
    struct dictionary *dict;

    dict = (struct dictionary *) malloc(sizeof(struct dictionary));
    check_ptr(dict);
    //set dictionary name
    if(dict_name == NULL)
        dict->name = DEF_DICT;
    else
        dict->name = dict_name;
    
    if(!(fd = fopen(dict->name,"r"))) raise_error("no dictionary");
    //ineffeciently loop twice toget len and then words
    while(getline(&buffer, &buf_size, fd) != -1){
        buffer = NULL;
        buf_size = 0;
        len++;
    }
    //set back to start pointer in file
    fseek(fd,0L,SEEK_SET);
    // get nume of entries
    dict->entries = len;

    //malloc and set words to  dictionary
    dict->word_list = (char **)malloc(len*sizeof (char*));
    check_ptr(dict->word_list);
    while(getline(&buffer, &buf_size, fd) != -1){
        dict->word_list[pos] = buffer;
        buffer = NULL;
        buf_size = 0;
        pos++;
    }
    fclose(fd);
    return dict;
}
//print client IP
void print_peerip(int socketfd){
    //setip peer sock 
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    char clientip[20];
    //get ip
    int tempfd = getpeername(socketfd,(struct sockaddr *)&peer_addr, &addr_len);
    //convert from network to readbable bits
    strcpy(clientip, inet_ntoa(peer_addr.sin_addr));
    printf("Client %s with FD %d - Added\n",clientip,socketfd);
}
//print all host's listening IP
void print_hostip(){
    //get ifaddrs
    struct ifaddrs *local; // needs this to free later
    struct sockaddr_in *sock_local;
    getifaddrs(&local);
    struct ifaddrs *temp = local;//loop through each address on host

    //not null
    while(temp){
        //if IPv4
        if (temp->ifa_addr && temp->ifa_addr->sa_family == AF_INET){
            sock_local = (struct sockaddr_in *) temp->ifa_addr;
            printf("\t%s: %s\n", temp->ifa_name, inet_ntoa(sock_local->sin_addr));
        }
        // get all 
        temp = temp->ifa_next;
    }
    // freeing to prevent mem leak
    freeifaddrs(local);
}
//remove \r \n \t to clean up the word
char *remove_n(char *word, int word_len){
    char *temp = (char *)malloc(sizeof(char)*(word_len+1));
    check_ptr(temp);
    int pos =0;
    int i;
    // go through for all \n or \r or \t, remove them
    for(i = 0; i < word_len; i++){
        if((word[i] != '\n') && (word[i] != '\r') && (word[i] != '\t')){
            temp[pos] = word[i];
            pos++;
        }
    }
    //format at to compare
    temp[pos] = '\n';
    temp[pos+1] = '\0';
    return temp;
}