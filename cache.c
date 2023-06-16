#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;
static int cache_created;

int cache_create(int num_entries) {
  if(cache_size > 0){
    return -1;
  }
  if(num_entries < 2 || num_entries > 4096){
    return -1;
  }
  //checks invalid parameters
  cache = calloc(num_entries,sizeof(cache_entry_t));
  cache_size = num_entries;
  clock = 0;
  cache_created = 1;
  //creates cache, intialize's clock, updated cache size and sets global variable to check when cache created or not.
  return 1;
}


int cache_destroy(void) {
  if(cache_size == 0){
    return -1;
  }
  //if there is no cache, can't remove
  free(cache);
  cache = NULL;
  cache_size = 0; 
  cache_created = 0;
  clock = 0;
  //reset cache,size,clock,global var for created
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  if(cache_created == 0){
    return -1;
  }
  if(cache == NULL){
    return -1;
  }
  if(buf == NULL){
    return -1;
  }
  //check for invalid parameters i.e cache created and buff and cache contains items
  num_queries += 1;
  //each time lookup is called,increment queries
  for(int i = 0; i < cache_size; i++){  
    if(cache[i].valid == true){
      if(cache[i].disk_num == disk_num){
	      if(cache[i].block_num == block_num){
          num_hits += 1;
          clock += 1;
          cache[i].access_time = clock;
          memcpy(buf,cache[i].block,256);
	        return 1;
        }
        //for each element in the cache check to see if the element in cache is element were looking for
        //if this is true then we should mark a hit, update clock, update contents, and incremenet clock 
        //because our operation has been completed
      }
    }
  }
  return -1;
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  for(int i = 0; i < cache_size; i++){
    if(cache[i].valid == true){
      if(cache[i].disk_num == disk_num){
        if(cache[i].block_num == block_num){
          memcpy(cache[i].block,buf,256);
          clock += 1;
          cache[i].access_time = clock;
        }
        //finds the element in the cache were looking for and updates block and clock
      }
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  if(cache_created == 0){
    return -1;
  }
  if(cache == NULL){
    return -1;
  }
  if(buf == NULL){
    return -1;
  }
  if(disk_num < 0 || disk_num > 15){
    return -1;
  }
  if(block_num < 0 || block_num > 255){
    return -1;
  }
  //invalid parameters
  for(int i = 0; i < cache_size; i++){
    if(cache[i].valid == true){
      if(cache[i].disk_num == disk_num){
        if(cache[i].block_num == block_num){
	        return -1;
	      }
      }
    }
  }
   //we can't insert if the disk and block is already there
	     
  for(int j = 0; j < cache_size; j++){
    if(cache[j].valid == false){
      cache[j].valid = true;
      cache[j].disk_num = disk_num;
      cache[j].block_num = block_num;
      cache[j].access_time += clock;
      memcpy(cache[j].block,buf,256);
      return 1;
    }
  }
  //if empty space in cache set element into cache
  
  int LRU = 0;
  for(int k = 0; k < cache_size; k++){
    if(cache[k].valid == true){
      if(cache[k].access_time < cache[LRU].access_time){
        LRU = k;
      }
    }
  }
  //finds LRU by comparing access times
  cache[LRU].block_num = block_num;
  cache[LRU].disk_num = disk_num;
  clock += 1;
  cache[LRU].access_time += clock;
  memcpy(cache[LRU].block,buf,256);
  //Evicts the LRU by update the disk block time and stored data
  return 1; 
}

bool cache_enabled(void) {
  if(cache_created == 1){
    return true;
  }
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}
