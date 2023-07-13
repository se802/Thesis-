//
// Created by csdeptucy on 7/8/23.
//

#include "RegInode.h"



class Counter {
public:
  u_int64_t count = 0;
};


int RegInode::allocate_space(uint64_t blockID, std::atomic<size_t >&avail_bytes)
{
  //if (avail_bytes < BLOCK_SIZE) return -ENOSPC;
  //avail_bytes -= BLOCK_SIZE;
  data_blocks.emplace(blockID, make_cown<Block>(BLOCK_SIZE,blockID));
  return 0;
}


class Wrapper {
public:
  Wrapper(cown_ptr<Block>& ptr, int bytes_to_write, int block_offset, int bytes_written,int total_bytes,const char *buf)
  : ptr_(ptr), bytes_to_write_(bytes_to_write), block_offset_(block_offset), bytes_written_(bytes_written), read_buf_(buf) {}


public:
  cown_ptr<Block> ptr_;
  int bytes_to_write_;
  int block_offset_;
  int bytes_written_;
  const char *read_buf_;
};




void batch_write(const std::vector<Wrapper>& blocks,fuse_req_t req,size_t size,char *ptr)
{
  if (blocks.size() == 1)
  {
    when(blocks[0].ptr_) << [=](acquired_cown<Block> blk0) {
      blk0->write(blocks[0].read_buf_,blocks[0].bytes_to_write_,blocks[0].block_offset_,blocks[0].bytes_written_);
      free((void*)ptr);
    };
  }

  if (blocks.size() == 2)
  {
    when(blocks[0].ptr_, blocks[1].ptr_) << [=](acquired_cown<Block> blk0, acquired_cown<Block> blk1) {
      blk0->write(blocks[0].read_buf_,blocks[0].bytes_to_write_,blocks[0].block_offset_,blocks[0].bytes_written_);
      blk1->write(blocks[1].read_buf_,blocks[1].bytes_to_write_,blocks[1].block_offset_,blocks[1].bytes_written_);
      free((void*)ptr);
    };
  }
}

void RegInode::write(const char* buf, size_t size, off_t offset,fuse_req_t req,std::atomic<size_t >&avail_bytes, char *ptr)
{
  auto now = std::time(nullptr);
  i_st.st_ctime = now;
  i_st.st_mtime = now;
  // Calculate the block ID based on the offset
  u_int64_t bytes_written = 0;
  u_int64_t blockId = offset/ BLOCK_SIZE;
  u_int64_t block_offset;
  u_int64_t remaining_size = size;

  off_t var = offset+size;
  i_st.st_size = std::max(i_st.st_size, var);

  std::vector<Wrapper> vec_array;

  while (remaining_size>0)
  {
    blockId = (offset + bytes_written) / BLOCK_SIZE;
    block_offset = (offset + bytes_written) % BLOCK_SIZE;

    if (data_blocks.find(blockId) == data_blocks.end())
    {
      data_blocks.emplace(blockId, make_cown<Block>(BLOCK_SIZE,blockId));
      int ret = allocate_space(blockId,avail_bytes);

      //if (ret){
      //  fuse_reply_err(req,-ret);
      //  exit(-99);
      //}

      i_st.st_blocks = std::min(i_st.st_blocks+1,(__blkcnt_t)data_blocks.size());
    }

    size_t bytes_to_write = std::min(BLOCK_SIZE - block_offset, remaining_size);
    cown_ptr<Block> blk = data_blocks.at(blockId);

    Wrapper wrapper(blk,bytes_to_write,block_offset,bytes_written,size,ptr);
    vec_array.push_back(wrapper);

    bytes_written += bytes_to_write;
    remaining_size -= bytes_to_write;
  }
  batch_write(vec_array,req,size,ptr);
}





//FIXME: What if someone does lseek(fd,1000,SEEK_CUR) --> write(fd,"a",1), and someone tries to read from offset 0? what will it return
int RegInode::read( size_t size, off_t offset, fuse_req_t req)
{

  // Calculate the block ID based on the offset
  u_int64_t bytes_read = 0;
  u_int64_t blockId;
  u_int64_t block_offset;
  long remaining_size = std::min((long)size, (long)i_st.st_size -offset );
  remaining_size = std::max(remaining_size,0l);

  if(remaining_size == 0)
  {
    fuse_reply_buf(req, NULL, 0);
    return 0;
  }



  u_int64_t total = remaining_size;

  char *buf = static_cast<char*>(malloc(sizeof(char) * remaining_size));


  auto counter = make_cown<Counter>();
  while (remaining_size > 0)
  {
    blockId = (offset + bytes_read) / BLOCK_SIZE;
    block_offset = (offset + bytes_read) % BLOCK_SIZE;

    if(data_blocks.find(blockId) == data_blocks.end()){
      bytes_read += BLOCK_SIZE;
      remaining_size -= BLOCK_SIZE;
      continue ;
      std::cout << "Exiting in read" << std::endl;
      exit(-99);
    }
    cown_ptr<Block> blk = data_blocks.at(blockId);
    size_t bytes_to_read = std::min((long)(BLOCK_SIZE - block_offset), remaining_size);
    //printf("offset: %ld, bytes_read %lu , size:%zud, bytes_to_read %lu, file_size: %ld, remaining size %ld\n",offset, bytes_read,size,bytes_to_read,i_st.st_size,remaining_size);

    when(blk) << [=] (acquired_cown<Block> blk){
      blk->read(buf, bytes_to_read, block_offset, bytes_read);

      when(counter) << [=](auto ctr){
        ctr->count += bytes_to_read;
        //printf("ctr: %d, total: %d\n",ctr->count,total);
        if(ctr->count == total)
        {
          //printf("ctr: %d, total: %d\n",ctr->count,total);
          int ret = ctr->count;
          if (ret >= 0)
            fuse_reply_buf(req, buf, ret);
          else
            fuse_reply_err(req, -ret);

          free(buf);
        }
        return 0;
      };
    };
    bytes_read += bytes_to_read;
    remaining_size -= bytes_to_read;
  }
  return 0;
  //printf("offset: %ld, bytes_read %lu, size:%zud, bytes_read %lu, file_size: %ld, remaining size %ld\n",offset,bytes_read ,size,bytes_read,i_st.st_size,remaining_size);

}

RegInode::~RegInode() {
  std::cout << "hey" << std::endl;
};
