// Copyright Microsoft and Project Verona Contributors.
// SPDX-License-Identifier: MIT

#define FUSE_USE_VERSION 34

#include "fuse3/fuse_lowlevel.h"

#include <cpp/when.h>
#include <debug/harness.h>
#include <map>

#define BLOCK_SIZE 32

class Body
{
public:
  ~Body()
  {
    Logging::cout() << "Body destroyed" << Logging::endl;
  }
};

using namespace verona::cpp;

struct Block {
  Block(size_t size)
  : size(size)
    , buf(new char[size]) {

      memset(buf.get(),0xff,size);

  }

  size_t size;
  std::unique_ptr<char[]> buf;

  void write(char* data, uint64_t bytes_to_write, uint64_t block_offset,uint64_t bytes_written);
  void read(char* data, uint64_t bytes_to_read, uint64_t block_offset,uint64_t bytes_read);
};

void Block::write(char* data, uint64_t bytes_to_write, uint64_t block_offset,uint64_t bytes_written)
{
  memcpy(buf.get()+block_offset,data+bytes_written,bytes_to_write);
}

void Block::read(char* data, uint64_t bytes_to_read, uint64_t block_offset, uint64_t bytes_read)
{
  memcpy(data+bytes_read,buf.get()+block_offset,bytes_to_read);
}

class Inode {
public:
  Inode(
    fuse_ino_t ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    mode_t mode)
  : ino(ino)
     {
    memset(&i_st, 0, sizeof(i_st));
    i_st.st_ino = ino;
    i_st.st_atime = time;
    i_st.st_mtime = time;
    i_st.st_ctime = time;
    i_st.st_uid = uid;
    i_st.st_gid = gid;
    i_st.st_blksize = blksize;
  }

  virtual ~Inode() = 0;

  const fuse_ino_t ino;

  struct stat i_st;

  bool is_regular() const;
  bool is_directory() const;
  bool is_symlink() const;

  long int krefs = 0;

protected:

};

class RegInode : public Inode {
public:
  RegInode(
    fuse_ino_t ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    mode_t mode)
  : Inode(ino, time, uid, gid, blksize, mode) {
    i_st.st_nlink = 1;
    i_st.st_mode = S_IFREG | mode;
  }

  ~RegInode();

  int write(char *buf,size_t size,off_t offset,fuse_req_t req);
  int read(char *buf,size_t size,off_t offset,fuse_req_t req);
  std::map<off_t, Block> data_blocks;
};


class Counter {
public:
  u_int64_t count = 0;
};

// Someone acquires a regular inode and writes through it
// when (regInode) << []() { ... regInode.write() ... }
int RegInode::write(char* buf, size_t size, off_t offset,fuse_req_t req)
{

  auto now = std::time(nullptr);
  i_st.st_ctime = now;
  i_st.st_mtime = now;

  // Calculate the block ID based on the offset
  u_int64_t bytes_written = 0;
  u_int64_t blockId = offset/ BLOCK_SIZE;
  u_int64_t block_offset;
  u_int64_t remaining_size = size;

  // FIXME: HOW TO FREE MEMORY FROM A COWN OBJECT?
  auto counter = make_cown<Counter>();

  while (remaining_size>0)
  {
    blockId = (offset + bytes_written) / BLOCK_SIZE;
    block_offset = (offset + bytes_written) % BLOCK_SIZE;


    if (data_blocks.find(blockId) == data_blocks.end())
    {
      // Since only one can hold reg inode space can be safely allocated for that inode
      data_blocks.emplace(blockId, Block(BLOCK_SIZE));
      // FIXME AVAILABLE SPACE COUNTER SHOULD BE A COWN
      // FIXME: BEFORE ALLOCATING BLOCK CHECK THAT THERE IS AVAILABLE SPACE AND RETURN ERROR
      // int ret = ...
      // fuse_reply_err(req, -ret);
    }

    size_t bytes_to_write = std::min(BLOCK_SIZE - block_offset, remaining_size);

    // TODO: Last time we said that we could have multiple blocks within a when,
    //  but the only way to do this would be to collect all cowns together somehow, but how?
    int id = blockId;

    Block & blk = data_blocks.at(id); // Use a reference
    blk.write(buf,bytes_to_write,block_offset,bytes_written);

    //when(blk,counter) << [=] (acquired_cown<Block> blk,auto ctr){
    //  blk->write(buf,bytes_to_write,block_offset,bytes_written);
    //  //memcpy(data_blocks.at(blockId).buf.get()+block_offset,buf+bytes_written,bytes_to_write);
    //  ctr->count += bytes_to_write;
    //  //printf("Writting in block %d\n",blockId);
    //};
    bytes_written += bytes_to_write;
    remaining_size -= bytes_to_write;
  }
  off_t var = offset+size;
  i_st.st_size = std::max(i_st.st_size,var);

  return bytes_written;
  // TODO: PROBLEM IS THAT WE HAVE TO REPLY TO THE KERNEL WHEN THE FOR LOOP FINISHES
  //  USING THIS HACK I MANAGED TO SERIALIZE THE EXECUTION OF THE LOOP, IS THAT FINE?
  //when(counter) << [=] (acquired_cown<Counter> ctr){
  //  if (ctr->count == size) {
  //    printf("Done \n");
  //    // All writes have completed, send the reply to the kernel
  //    //fuse_reply_write(req, ctr->count);
  //  }
  //};

  //if (ret >= 0)
  //  fuse_reply_write(req, ret);
  //else
  //  fuse_reply_err(req, -ret);
}

int RegInode::read(char* buf, size_t size, off_t offset, fuse_req_t req)
{

  memset(buf, 0, size);

  // Calculate the block ID based on the offset
  u_int64_t bytes_read = 0;
  u_int64_t blockId = offset / BLOCK_SIZE;
  u_int64_t block_offset;
  u_int64_t remaining_size = size;
  //sleep(5);
  // Create a Counter cown for counting bytes read
  auto counter = make_cown<Counter>();

  while (remaining_size > 0)
  {
    blockId = (offset + bytes_read) / BLOCK_SIZE;
    block_offset = (offset + bytes_read) % BLOCK_SIZE;

    // Check if block exists
    if (data_blocks.find(blockId) == data_blocks.end())
    {
      // Return an error if block does not exist
      //reply_read(bytes_read,req,buf);
      return bytes_read;
      break ;
    }

    size_t bytes_to_read = std::min(BLOCK_SIZE - block_offset, remaining_size);
    auto x = i_st.st_size-offset+bytes_read;
    //Reached EOF
    bool reached_EOF = false;
    if(i_st.st_size-offset+bytes_read<bytes_to_read){
      remaining_size = 0;
      reached_EOF = true;
    }
    bytes_to_read = std::min(bytes_to_read,i_st.st_size-offset+bytes_read);
    int id = blockId;
    Block &blk = data_blocks.at(id);
    blk.read(buf, bytes_to_read, block_offset, bytes_read);
    //when(blk, counter) << [=] (acquired_cown<Block> blk, auto ctr){
    //  blk->read(buf, bytes_to_read, block_offset, bytes_read);
    //  ctr->count += bytes_to_read;
    //};
    bytes_read += bytes_to_read;
    remaining_size -= bytes_to_read;
  }

  // Reply to the kernel when all reads have completed
  //when(counter) << [=] (acquired_cown<Counter> ctr){
  //  if (ctr->count == size) {
  //    printf("str %s\n",buf);
  //    // All reads have completed, send the reply to the kernel
  //    //reply_read(bytes_read,req,buf);
  //  }
  //};
}


class DirInode : public Inode {
public:
  typedef std::map<std::string, uint64_t> dir_t;

  DirInode(
    fuse_ino_t ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    mode_t mode)
  : Inode(ino, time, uid, gid, blksize, mode) {
    i_st.st_nlink = 2;
    i_st.st_blocks = 1;
    i_st.st_mode = S_IFDIR | mode;
  }

  ~DirInode();

  dir_t dentries;
};


bool Inode::is_regular() const { return i_st.st_mode & S_IFREG; }

bool Inode::is_directory() const { return i_st.st_mode & S_IFDIR; }

bool Inode::is_symlink() const { return i_st.st_mode & S_IFLNK; }


Inode::~Inode() {
  std::cout << "hey" << std::endl;
}

RegInode::~RegInode() {
  std::cout << "hey" << std::endl;
};

DirInode::~DirInode() {
  std::cout << "hey" << std::endl;
};



void test_body()
{
  auto in = std::make_unique<RegInode>(5, 5, 5, 5, 4096, S_IFREG | 5);
  char buf[4096] = "this a string";

  int size = strlen(buf)+1;
  char buf2[4000];
  int write = in->write(buf, strlen(buf)+1,11500, reinterpret_cast<fuse_req_t>(5));
  int read = in->read(buf2,3000,11500,reinterpret_cast<fuse_req_t>(5));
  printf("str is %s\n",buf2);
  printf("read - write : %d - %d :",read,write);
}

int main(int argc, char** argv)
{
  test_body();
  exit(1);
  SystematicTestHarness harness(argc, argv);

  harness.run(test_body);

  return 0;
}
