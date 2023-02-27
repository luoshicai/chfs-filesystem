#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || !buf){
    return;
  }
  memcpy(buf,blocks[id],BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || !buf){
   return;
  }
  memcpy(blocks[id],buf,BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  uint32_t block_num;
  for (block_num = FILEBLOCK;block_num<sb.nblocks;++block_num){
    if (using_blocks[block_num]==false){
      using_blocks[block_num]=true;
      return block_num;
    }
  }
  //如果没有空余的block则直接返回
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  using_blocks[id]=false;
  return;
}

void inode_manager::alloc_nth_block(inode_t* ino,uint32_t n)
{
  char buf[BLOCK_SIZE];
  if (ino == NULL){
    exit(0);
  }
  if (n < NDIRECT){
    ino->blocks[n] = bm->alloc_block();
  }
  else if (n < MAXFILE){
    if (ino->blocks[NDIRECT]==0){
      ino->blocks[NDIRECT] = bm->alloc_block();
    }
    bm->read_block(ino->blocks[NDIRECT],buf);
    ((blockid_t*)buf)[n-NDIRECT] = bm->alloc_block();
    bm->write_block(ino->blocks[NDIRECT], buf);
  }
  else
    exit(0);
}

blockid_t inode_manager::get_nth_blockId(inode_t *ino, uint32_t n){
  char buf[BLOCK_SIZE];
  blockid_t blockId;

  if (n < NDIRECT){
    blockId = ino->blocks[n];
  }
  else if (n < MAXFILE){
    bm->read_block(ino->blocks[NDIRECT],buf);
    blockId = ((blockid_t*)buf)[n-NDIRECT];
  }
  else{
    exit(0);
  }
  return blockId;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  int inum = 1;
  for (; inum<=INODE_NUM; ++inum){
    inode_t *ino = get_inode(inum);
    if (ino==NULL){
      ino = (inode_t*)malloc(sizeof(inode_t));
      bzero(ino,sizeof(inode_t));
      ino->type = type;
      ino->atime = time(NULL);
      ino->mtime = time(NULL);
      ino->ctime = time(NULL);
      put_inode(inum,ino);
      free(ino);
      break;
    }
    free(ino);
  }
  if (inum>INODE_NUM){
    printf("Error: no inode numbers avaliable!\n");
    exit(-1);
  }
  return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  inode_t *ino = get_inode(inum);
  if (ino!=NULL){
    if (ino->type==0){
      return;
    }
    else{
      bzero(ino,sizeof(inode_t));
      put_inode(inum,ino);
      free(ino);
    }
  }
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino;
  /* 
   * your code goes here.
   */
  char buf[BLOCK_SIZE];
  struct inode *inode_disk;


  if (inum < 0 || inum >= INODE_NUM){
    printf("\tim: inum out of range\n");
    return NULL;
  } 

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);

  inode_disk = (struct inode*)buf + inum%IPB;  //IPB=1,inum%IPB=0

  if (inode_disk->type == 0){
    //The inode does not exist, has not been allocated, and returns null
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *inode_disk;
  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  uint32_t block_num;
  uint32_t remain_size;
  char buf[BLOCK_SIZE];

  inode_t* ino = get_inode(inum);
  if (ino!=NULL){
    *size = ino->size;
    *buf_out = (char*)malloc(*size);

    block_num = ino->size/BLOCK_SIZE;
    remain_size = ino->size%BLOCK_SIZE;

    for (uint32_t i=0; i<block_num; ++i){
      bm->read_block(get_nth_blockId(ino,i), buf);
      memcpy(*buf_out + i*BLOCK_SIZE, buf, BLOCK_SIZE);
    }
    if (remain_size>0){
      bm->read_block(get_nth_blockId(ino,block_num),buf);
      memcpy(*buf_out + block_num*BLOCK_SIZE, buf, remain_size);
    }
    free(ino);
  }
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  uint32_t old_block_num,new_block_num,remain_size;
  uint32_t block_num;
  //判断size是否超过了一个文件的最大大小
  if (size < 0||size > MAXFILE*BLOCK_SIZE){
     exit(0);
  }
  inode* ino = get_inode(inum);
  if (ino!=NULL){
    //比较写入后和写入前的大小，释放掉多余的block，申请不够的block
    old_block_num = ino->size == 0 ? 0 : (ino->size-1)/BLOCK_SIZE + 1;
    new_block_num = size == 0 ? 0 : (size-1)/BLOCK_SIZE +1;

    if (old_block_num < new_block_num){
      for (uint32_t i=old_block_num;i<new_block_num;++i){
        alloc_nth_block(ino,i);
      }
    }
    else if (old_block_num > new_block_num){
      for (uint32_t i=old_block_num; i<new_block_num; ++i){
        bm->free_block(get_nth_blockId(ino,i));
      }
    }
    block_num = size/BLOCK_SIZE;
    remain_size = size%BLOCK_SIZE;

    for (uint32_t i=0; i<block_num; ++i){
      bm->write_block(get_nth_blockId(ino,i), buf + i*BLOCK_SIZE);
    }
    if (remain_size>0){
      char tmp[BLOCK_SIZE];
      memcpy(tmp, buf+block_num*BLOCK_SIZE, remain_size);
      bm->write_block(get_nth_blockId(ino,block_num),tmp);
    }
    
    ino->size = size;
    ino->atime = time(NULL);
    ino->mtime = time(NULL);
    ino->ctime = time(NULL);
    put_inode(inum, ino);
    free(ino);
  }
  return;
}

void
inode_manager::get_attr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode_t* ino = get_inode(inum);
  if (ino == NULL){
    return;
  }
  
  a.type = ino->type;
  a.size = ino->size;
  a.atime = ino->atime;
  a.mtime = ino->mtime;
  a.ctime = ino->ctime;

  free(ino);
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  inode_t *ino = get_inode(inum);
  if (ino!=NULL){
    uint32_t block_num = ino->size == 0 ? 0 : (ino->size-1)/BLOCK_SIZE + 1;
    for (uint32_t i =0; i < block_num; ++i){
      bm->free_block(get_nth_blockId(ino,i));
    }
    if (block_num > NDIRECT){
      bm -> free_block(ino->blocks[NDIRECT]);
    }
    free_inode(inum);
    free(ino);
  }
  return;
}
