#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n bytes from fd; returns true on success and false on
 * failure */
static bool nread(int fd, int len, uint8_t *buf) {
  //read the bytes from the sockets 
  int num_read = 0;
  while(num_read < len){
    int r = read(fd, buf + num_read, len - num_read);
    if(r < 0){
      return false;
    }
    num_read += r;
    //loop iteraively reads the bytes needed until len satisfied
    //returns false if it can't read
  }
  return true;
}

/* attempts to write n bytes to fd; returns true on success and false on
 * failure */
static bool nwrite(int fd, int len, uint8_t *buf) { 
  //write the bytes required 
  int num_write = 0;
  while(num_write < len){
    int w = write(fd, buf + num_write, len - num_write);
    if(w < 0){
      return false;
    }
    num_write += w;
  }
  return true;
  //loop iteraively writes the bytes needed until len satisfied
  //returns false if it can't write
}

/* attempts to receive a packet from fd; returns true on success and false on
 * failure */
static bool recv_packet(int fd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  if(fd == -1){
    return false;
  }
  //check's for valid socket
  uint8_t *buf = malloc(HEADER_LEN);
  if(nread(fd, HEADER_LEN, buf) == false){
    return false;
  }
  uint16_t length;
  memcpy(&length, buf, 2);
  //Get the first two bytes from buf containing len
  length = ntohs(length);
  //converts len to host byte order

  uint16_t recv_op;
  memcpy(&recv_op, buf+2, 4);
  //get the next 4 bytes needed to get the op
  *op = ntohl(recv_op);
  //converts op to host byte order

  uint16_t recv_ret;
  memcpy(&recv_ret, buf+6, 2);
  //gets last 2 bytes needed for ret
  *ret = ntohs(recv_ret);
  //converts to host byte order

  if(length > HEADER_LEN){
    nread(fd, 256, block);
    //reads entire block
  }
  return true;
}

/* attempts to send a packet to sd; returns true on success and false on
 * failure */
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  if(sd == -1){
    return false;
  }
  uint16_t length = HEADER_LEN; 
  uint8_t *buf = malloc(HEADER_LEN + 256);
  if(op >> 26 == JBOD_WRITE_BLOCK){
    length += 256;
    memcpy(buf + HEADER_LEN, block, 256);
  }
  //Need to determine that we need to write and then retrieve the block

  uint16_t send_len = htons(length);
  memcpy(buf, &send_len, 2);
  //convert len to  network byte order and save

  uint32_t send_op = htonl(op);
  memcpy(buf + 2, &send_op, 4);
  //convert op to network byte order and save

  return nwrite(sd, length, buf);
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. */
bool jbod_connect(const char *ip, uint16_t port) {
  cli_sd = socket(PF_INET, SOCK_STREAM, 0);
  if(cli_sd == -1){
    printf("Error on socket creation");
    return false;
  }
  struct sockaddr_in caddr;
   //create the struct to convert
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  //converts the ipv4 structure to unix structure

  if(inet_aton(ip, &caddr.sin_addr) == 0){
    printf("Address or Port error");
    return false;
  }
  if(connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr))==-1){
    printf("Unable to connect");
    return false;
  }
  //Now complete we connect to server using calling the socket and address
  
  return true;
}


/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
 close(cli_sd);
 cli_sd = -1;
}

/* sends the JBOD operation to the server and receives and processes the
 * response. */
int jbod_client_operation(uint32_t op, uint8_t *block) {
  if(send_packet(cli_sd, op, block) == false){
    return -1;
  }
  uint16_t ret;
  if(recv_packet(cli_sd, &op, &ret, block) == false){
    return -1;
  }
  return ret;  
  //calling send and receive packet 
  //Needed true for operation in mdadm to be successful
}
