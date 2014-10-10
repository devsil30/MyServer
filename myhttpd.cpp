
const char * usage =
"                                                               \n"
"myhttpd-server:                                                \n"
"                                                               \n"
"                                                               \n"
"To use it in one window type:                                  \n"
"                                                               \n"
"   myhttpd [-f|-t|-p] <port>                                       \n"
"                                                               \n"
"Where 1024 < port < 65536.             \n"
"                                                               \n"
"                                                               \n";

/*@author: sill*/


/**
* This program was written because I was curious of how the apache and similiar systems managed a web server
* this application will run on most linux/unix systems and will handle multiple requests for a web page to be served by spawning new threads.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
int QueueLength = 5;
int defaultport = 1098; // default port in case 1 in not specified

static char * good_response = 
  "HTTP/1.1 200 OK\n"
  "Server: CS252 lab5\n"
  "Content-type: %s"
  ;


char * bad_request = "HTTP/1.1 404 File Not Found\n"
  "Content-type: text/html\n"
  "\n"
  "<html>\n"
  " <body>\n"
  "  <h1> Not Found </h1?\r\n"
  "  <p> The requested URL %s was not found on this server.</p>\n"
  " </body>\n"
  "</html>\n";


pthread_mutex_t mutex;

void processRequest( int socket );
void processResponse( int socket );
void handle_bad_request(int socket, const char* file);
void handle_good_request( int socket, const char* file);
void forkServ( int masterSocket);
void threadRequest( int slaveSocket);
void poolServ(int masterSocket);
void iterServ( int masterSocket);
int exists( const char* fname);


int
main( int argc, char ** argv )
{
  // Print usage if not enough arguments
  if ( argc < 2 ) {
    fprintf( stderr, "%s", usage );
    exit( -1 );
  }
  
  int port;
  // Get the port from the arguments
  if(argc == 3){ // concurrent
    port = atoi( argv[2] );
  }else{ // iterative server
    port = atoi( argv[1] );
  }
 

  // Set the IP address and port for this server
  struct sockaddr_in serverIPAddress; 
  memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
  serverIPAddress.sin_family = AF_INET;
  serverIPAddress.sin_addr.s_addr = INADDR_ANY;
  serverIPAddress.sin_port = htons((u_short) port);
  
  // Allocate a socket
  int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
  if ( masterSocket < 0) {
    perror("socket");
    exit( -1 );
  }
  //
  // Set socket options to reuse port. Otherwise we will
  // have to wait about 2 minutes before reusing the sae port number
  int optval = 1; 
  int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
		       (char *) &optval, sizeof( int ) );
  
 
  // Bind the socket to the IP address and port
  int error = bind( masterSocket,
		    (struct sockaddr *)&serverIPAddress,
		    sizeof(serverIPAddress) );
  if ( error ) {
    perror("bind");
    exit( -1 );
  }
  
  // Put socket in listening mode and set the 
  // size of the queue of unprocessed connections
  error = listen( masterSocket, QueueLength);
  if ( error ) {
    perror("listen");
    exit( -1 );
  }

  /*  check which concurrencey we use */
  if(argc == 3 && strncmp(argv[1], "-f",2)== 0) { // do fork
    printf("in fork\n");
    forkServ(masterSocket);
  }
  else if(argc == 3 && strncmp(argv[1], "-t",2) == 0) {
      printf("in thread server\n");
      
      while(1) {
	struct sockaddr_in clientInfo;
	int alen = sizeof(clientInfo);
	int slaveSocket = accept(masterSocket,(struct sockaddr *) &clientInfo, (socklen_t*) &alen);

	if(slaveSocket <0){
	  perror("accept");
	  exit(-1);
	}


	// init pthread_attr_t
	pthread_attr_t attr;
	pthread_attr_init(&attr);

	// create thread
	pthread_t thread;

	pthread_create(&thread,&attr,(void*(*)(void*))threadRequest, (void *)slaveSocket);


	pthread_exit(NULL);
      }// end while
  }
  else if( argc == 3 && strncmp(argv[1], "-p", 2) == 0) {
    printf("in thread pool serv\n");

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_mutex_init(&mutex,NULL);

    pthread_t threads[5];
    int i = 0;
    for(i; i< 0; i++) {
      // create threads
    pthread_create(threads, &attr, (void*(*)(void *)) poolServ, (void *)masterSocket);

    }// end for

    pthread_join(threads[0],NULL); // wait for server
  }
  else{// iterative server
    printf("iterative server \n");
    iterServ(masterSocket);
  }
}

void
processRequest( int fd )
{
  int n;

  // declare cwd
  char *cwd = new char[1024];
  
  
  unsigned char newchar;
  unsigned char lastchar = 0;

  /*parse GET request made my client 
   * GET <sp> <Document Requested> <sp> HTTP/1.0 <crlf>
   * <crlf>
   */
  int gotGet = 0;
  int docPath = 0;
  char curr_string[1024];
  //curr_string = (char*) malloc(sizeof(char));
  int len =0;
  char filePath[256];
  while((n = read(fd,&newchar, sizeof(newchar)))) {

	len++;
	if(newchar == ' '){
	  if(gotGet != 1){
	    gotGet = 1;
	    curr_string[len-1] = newchar;
	  }
	  else if(docPath != 1 ){
	    docPath = 1;
	    curr_string[len-1] = 0;
	    strcpy(filePath, curr_string);
	    //printf("file path: %s\n", filePath);
	  }
	}
	else if(newchar == '\n' && lastchar == '\r'){
	  break;
	}
	else{
	  lastchar = newchar;
	  curr_string[len -1]  = newchar;
	}     
  }   

  char reqPath[256];  // requested path from web client
   // Parse the Get request ignore GET and store request path in reqPath 
  sscanf(filePath,"%*s %s ", reqPath);

  cwd = getcwd(cwd, 1024);
  cwd = strcat(cwd, "/http-root-dir");


  //set path to requested path
  if(strncmp(reqPath,"/cgi-bin", 7) == 0){
    cwd = strcat(cwd, reqPath);
  }
  else{
    //cwd = getcwd(cwd, 1024);
    cwd = strcat(cwd, "/htdocs");
    cwd = strcat(cwd, reqPath);
  }
    
  
  if(strncmp(reqPath,"/\0",2) == 0 ){
    // cat index file path to cwd and call handle_good_request
    cwd = strcat(cwd,"index.html");
    handle_good_request(fd, cwd);
  }
  else if( exists(cwd) != 0){ 
    handle_good_request(fd, cwd);
  }
  else { // file does not exist call function ot handle bad request
    //printf("doesnt exist\n");
    handle_bad_request(fd, cwd);
  }
  

  return;
}

void handle_good_request(int fd, const char * file){
  // int page;
  FILE* page;
  int bytes_read;
  

  //buffer for message
  char headerBuff[256];
   
  const char* fileType = strchr(file, '.');
  //  printf("%s\n", fileType);
  if(strncmp(fileType, ".html", 5) == 0){
    snprintf(headerBuff,sizeof(headerBuff), good_response, "text/html\r\n\r\n");
    page = fopen(file,"r");
  }
  else if(strncmp(fileType, ".gif",4) == 0){
    snprintf(headerBuff,sizeof(headerBuff), good_response, "image/gif\r\n\r\n");
    page = fopen(file,"rb");
  }
  else if(strncmp(fileType, ".txt", 4) == 0) {
    snprintf(headerBuff,sizeof(headerBuff), good_response, "text/plain\r\n\r\n");
    page = fopen(file,"r");
  }
  if(page == NULL){
    perror("error on open");
  }



  /* write header response*/
  //write(fd,headerBuff,256);
  send(fd,headerBuff,256,0);

  fseek(page,0,SEEK_END); 
  int size = ftell(page);
  rewind(page);

  //printf("file size: %d\n", size);

  char *sendBuff; // create buffer to send

  /* malloc for sent buffer*/
  sendBuff = (char*) malloc (sizeof(char)*size);

  bytes_read = fread(sendBuff,1,size,page);
  //printf("bytes read: %d\n", bytes_read);
  // printf("Buffer to be read: %s\n", sendBuff);
  /* write to client document data*/
  int bytes_written;
  //bytes_written = write(fd, sendBuff, size);
  send(fd,sendBuff,bytes_read,0);
  

  // free(sendBuff);
  fclose(page);
} 


void handle_bad_request(int fd , const char * file){
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), bad_request, file );
  //printf("%s\n", buffer);
  write (fd, buffer, strlen(buffer));
}


void iterServ( int master){

  while(1){
    struct sockaddr_in clientInfo;
    int alen = sizeof(clientInfo);
    int slaveSocket = accept(master, (struct sockaddr *) &clientInfo, (socklen_t*) &alen);

    if(slaveSocket < 0){
      perror("accept");
      exit(-1);
    }

    processRequest(slaveSocket);

    shutdown(slaveSocket,2);
    close(slaveSocket);
  }

}
void forkServ( int master){

  while(1){

    // accept incoming connections
    struct sockaddr_in clientInfo;
    int alen = sizeof(clientInfo);
    int slaveSocket = accept(master,(struct sockaddr *) &clientInfo,(socklen_t*) &alen);

    if(slaveSocket < 0){
      perror("accept");
      exit(-1);
    }
    
    int slave = fork();
    if(slave == 0){
      
      processRequest(slaveSocket);
      
      shutdown(slaveSocket,2);
      
      
      close(slaveSocket);
      exit(0);
    }
    //shutdown(slaveSocket,2);
    //close(slaveSocket);
  }
}

void poolServ(int master) {

  while(1) {
    struct sockaddr_in clientInfo;
    int alen = sizeof(clientInfo);
    pthread_mutex_lock(&mutex);
    int slaveSocket = accept(master, (struct sockaddr *) &clientInfo, (socklen_t*) &alen);
    if(slaveSocket< 0) {
      continue;
    }  

    pthread_mutex_unlock(&mutex);

    processRequest(slaveSocket);
    close(slaveSocket);
  } // end while
}

void threadRequest(int socket){
  

  processRequest(socket);
  close(socket);

}
// Returns 1 on file found
// 0 if does not exist
int exists( const char *fname){

  FILE *file;
  if ( file = fopen(fname, "r")){
    fclose(file);
    return 1;
  }

  return 0;
}
