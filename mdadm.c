#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mdadm.h"
#include "jbod.h"



uint32_t encode_op(int cmd, int disk_num, int block_num) {
  uint32_t op = 0;
  op |= (cmd << 26)|(disk_num << 22)|(block_num);
  return op;
}
//operation function given in class to do required bit operations
//added bit shifting needed for disk and block


int mounted = 0;
//global variable to check if mounted or not
int mdadm_mount(void) {
  if(mounted == 1){
    return -1;
  }
  //can't mount if already mounted
  uint32_t op = encode_op(JBOD_MOUNT,0,0);
  int mount = jbod_client_operation(op,NULL);
  //call bitwise operation to get correct pass through got jbod operation
  if (mount == -1) return -1;
  
  //if jbod mounting operation unsuccessful it must fail
  mounted = 1;
  return 1;
}


int mdadm_unmount(void) {
  if(mounted == 0) return -1;
  
  uint32_t op = encode_op(JBOD_UNMOUNT, 0, 0);
  int mount = jbod_client_operation(op, NULL);
  if(mount == -1){
    return -1;
  }
  mounted = 0;
  return 1;
  //function works the same as mount
}
int min(int x,int y) {
  if(x > y){ 
    return y;
  }
  return x;
}
//min function used to find read block bytes given in piazza
void translate_address(uint32_t addr, int *block_num, int *disk_num, int *offset){
  *disk_num = addr / 65536;
  *block_num = (addr % 65536) / 256;
  *offset = (addr % 65536) % 256;

  //translated address based on pdf data, disk number simply address divided by total number of disk bytes
  //block number calculated by reamining disk bytes divided by total number of blocks
  //offset is the bytes needed to start at the address within the block 
}


void seek(int disk_num, int block_num){
  int seekdisk = encode_op(JBOD_SEEK_TO_DISK, disk_num, 0);
  jbod_client_operation(seekdisk, NULL);
  int seekblock = encode_op(JBOD_SEEK_TO_BLOCK, 0, block_num);
  jbod_client_operation(seekblock, NULL);
  //seeking disk and block using op translations and then performing operation
}

int disk_num = 0;
int block_num = 0;
int offset = 0;
int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  //int readlen = len;
  if(mounted == 0){
    return -1;
  }
  if(addr + len > 1048576){
    return -1;
  }
  if(len > 1024){
    return -1;
  }
  if(buf == NULL && len != 0){
    return -1;
  }
  uint8_t mybuf[256];
  uint32_t current_addr = addr;
  int remainbytes = 0;
  //current_addr keeps track of the current address,mybuf stores the contents copied from the block 
  // and remain keeps track of the remaining bytes. 
  int read_from_block = 0;

  while (current_addr < addr + len) {
    translate_address(current_addr,&block_num,&disk_num,&offset);
    //translate address used to find the block disk and offset needed at the current address we are at
    seek(disk_num, block_num);
    if(cache_enabled()){
      if(cache_lookup(disk_num, block_num, mybuf) == -1) {
        jbod_client_operation(encode_op(JBOD_READ_BLOCK,0,0),mybuf);
        cache_insert(disk_num, block_num, mybuf);
        }
      }
      //if the chache is enabled then we will check if it is in the cache if it is not we will read and insert
      //else we will simply read 
    else{
        jbod_client_operation(encode_op(JBOD_READ_BLOCK, 0, 0), mybuf);
    }
    //we then seek and read
    read_from_block = min(len, min(256, 256 - offset)); 
    if(current_addr == addr){
      //if were just starting the read
      memcpy(buf + remainbytes, mybuf + offset, read_from_block);
      remainbytes += read_from_block;
      current_addr += read_from_block;
    }
    else if(current_addr + 256 <= addr + len){
      //another if statement checking if we can read within the block however 
      //not bounded by starting address but typically where we finish is here
      memcpy(buf + remainbytes, mybuf + offset, read_from_block);
      remainbytes += read_from_block;
      current_addr += read_from_block;
    }
    else if (current_addr + 256 >= addr + len) {
      //another if statement for if we only need to read the next block we read what we need here and then finished 
	    memcpy(buf + remainbytes, mybuf, len - remainbytes);
	    break;
    }
    else{
      //only remaining statement is if we need to read across multiple blocks which is what we do here
	    memcpy(buf+remainbytes, mybuf,256);
	    remainbytes += 256;
	    current_addr += 256;
    }
  }
  return len;
}


int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  if(mounted == 0){
    return -1;
  }
  if(addr + len > 1048576){
    return -1;
  }
  if(len > 1024){
    return -1;
  }
  if(buf == NULL && len != 0){
    return -1;
  }
  uint8_t mybuf[256];
  uint32_t current_addr = addr;
  int remainbytes = 0;
  //current_addr keeps track of the current address,mybuf stores the contents copied from the block 
  // and remain keeps track of the remaining bytes. 
  int read_from_block = 0;

  while (current_addr < addr + len) {
    translate_address(current_addr,&block_num,&disk_num,&offset);
    //translate address used to find the block disk and offset needed at the current address we are at
    seek(disk_num, block_num);
    if(cache_enabled()){
      if(cache_lookup(disk_num, block_num, mybuf) == -1) {
        jbod_client_operation(encode_op(JBOD_READ_BLOCK,0,0),mybuf);
        cache_insert(disk_num, block_num, mybuf);
        //if the chache is enabled then we will check if it is in the cache if it is not we will read and insert
        //else we will simply read 
      }
    }
    else{
      jbod_client_operation(encode_op(JBOD_READ_BLOCK,0,0),mybuf);
    }
    //we then seek and read 
    seek(disk_num, block_num);
    //we then seek again so we can write later
    if(current_addr == addr){
      //if were just starting the read
      read_from_block = min(len, min(256, 256 - offset));
      memcpy(mybuf + offset,buf + remainbytes, read_from_block);
      jbod_client_operation(encode_op(JBOD_WRITE_BLOCK,0,0),mybuf);
      remainbytes += read_from_block;
      current_addr += read_from_block;
    }
    else if(current_addr + 256 <= addr + len){
      //another if statement checking if we can read within the block however 
      //not bounded by starting address but typically where we finish is here
      read_from_block = min(len, min(256, 256 - offset));
      memcpy(mybuf + offset, buf + remainbytes, read_from_block);
      jbod_client_operation(encode_op(JBOD_WRITE_BLOCK,0,0),mybuf);
      remainbytes += read_from_block;
      current_addr += read_from_block;
    }
    else if (current_addr + 256 >= addr + len) {
      //another if statement for if we only need to read the next block we read what we need here and then finished 
	    memcpy(mybuf, buf + remainbytes, len - remainbytes);
      jbod_client_operation(encode_op(JBOD_WRITE_BLOCK,0,0),mybuf);
	    break;
    }
    else{
      //only remaining statement is if we need to read across multiple blocks which is what we do here
	    memcpy(mybuf, buf+remainbytes, 256);
      jbod_client_operation(encode_op(JBOD_WRITE_BLOCK,0,0),mybuf);
	    remainbytes += 256;
	    current_addr += 256;
    }
  }
  return len;
  //In order to write I used the same methodology to seek disk and block and gather the correct information
  //The only difference is I swappeed the parameters of the memcpy because were writing the block with the contents of mybuf
  //once copying was completing we then write that information
}