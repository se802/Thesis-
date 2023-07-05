
/** @file
 *
 * minimal example filesystem using low-level API
 *
 * Compile with:
 *     gcc -Wall hello_ll.c `pkg-config fuse3 --cflags --libs` -o hello_ll
 *     ./hello_ll -f /tmp/ssfs
 */

//TODO 2/6/23: SUBMIT REPORT, Fix leaks in all callbacks
// Fixed: read,write,getattr
// DID 1. Changed block size (can affect performance)
//       2. Changed fuse_session_loop, dont use when to process_buf
//       3. Changed libfuse, struct fuse_req now contains a pointer to buf->mem which was leading to leak
//       4. changed fio to use multiple processes to write to same file --> improved bw https://www.flamingbytes.com/blog/fio-benchmark-on-multiple-files/


//FIXME:
//      1. Change rename and setattr
//      2. Get rid off FileHandle

#include "main.h"

#include "dirent.h"

#include <fuse3/fuse_opt.h>
#include "pthread.h"

#include <cassert>

#include <cerrno>
#include <fcntl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#define BLOCK_SIZE 4096

//32768



// -----------------------------------------------------------
// ------------------------- Definitions ---------------------
using namespace verona::cpp;
struct ExternalSource;

struct FileSystem;


struct Block {
  explicit Block(size_t size,int block_number)
  : size(size),block_number(block_number)
    , buf(new char[size]) {
    memset(buf.get(),0,size);
  }

  size_t size;
  int block_number = -1;
  std::unique_ptr<char[]> buf;


  void write(const char* data, uint64_t bytes_to_write, uint64_t block_offset,uint64_t bytes_written) const;
  void read(char* data, uint64_t bytes_to_read, uint64_t block_offset,uint64_t bytes_read) const;
};



void Block::write(const char* data, uint64_t bytes_to_write, uint64_t block_offset,uint64_t bytes_written) const
{
  memcpy(buf.get()+block_offset,data+bytes_written,bytes_to_write);
}

void Block::read(char* data, uint64_t bytes_to_read, uint64_t block_offset, uint64_t bytes_read) const
{
  memcpy(data+bytes_read, buf.get()+block_offset, bytes_to_read);

  // Create a new string with the copied data
  //char *copied_string = (char*)malloc((bytes_to_read + 1) * sizeof(char));
  //memcpy(copied_string, buf.get()+block_offset, bytes_to_read);
  //copied_string[bytes_to_read] = '\0';  // Null-terminate the new string
  //
  //printf("Bytes %lu, Copied substring: '%s'\n", bytes_to_read,copied_string);
  //
  //free(copied_string);  // Don't forget to free the allocated memory

}

class Inode {
public:
  Inode(
    fuse_ino_t ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    FileSystem* fs)
  : ino(ino)
    , fs_(fs) {
    memset(&i_st, 0, sizeof(struct stat));
    i_st.st_ino = ino;
    i_st.st_atime = time;
    i_st.st_mtime = time;
    i_st.st_ctime = time;
    i_st.st_uid = uid;
    i_st.st_gid = gid;
    i_st.st_blksize = blksize;



  }

  virtual ~Inode() = 0;

  fuse_ino_t ino;

  struct stat i_st;

  bool is_regular() const;


  long int krefs = 0;

protected:
  FileSystem* fs_;
};

class RegInode : public Inode {
public:
  RegInode(
    fuse_ino_t &ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    mode_t mode,
    FileSystem* fs)
  : Inode(reinterpret_cast<fuse_ino_t >(this), time, uid, gid, blksize,  fs) {
    i_st.st_nlink = 1;
    i_st.st_size = 0;
    i_st.st_mode = S_IFREG | mode;
    ino = reinterpret_cast<fuse_ino_t >(this);
  }

  // Default constructor
  RegInode() : Inode(-1, -1, -1, -1, -1, nullptr) {
    // Initialize other members if needed
    i_st.st_nlink = -1;
    i_st.st_size = -1;
    i_st.st_mode = S_IFREG;
    fake = true;
  }

  bool fake = false;

  ~RegInode() override;

  void write(const char *buf,size_t size,off_t offset,fuse_req_t req, std::atomic<size_t >&avail_bytes, int *ptr);
  int read(size_t size,off_t offset,fuse_req_t req);
  int allocate_space(uint64_t blockID, std::atomic<size_t> &avail_bytes);
  std::map<off_t, cown_ptr<Block>> data_blocks;
};


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

void batch_write(const std::vector<Wrapper>& blocks)
{
  if (blocks.size() == 1) {
    when(blocks[0].ptr_) << [=](acquired_cown<Block> blk0) {
      blk0->write(blocks[0].read_buf_,blocks[0].bytes_to_write_,blocks[0].block_offset_,blocks[0].bytes_written_);
      //std::cout << "Freeing memory1: " << static_cast<const void *>(blocks[0].read_buf_ ) << std::endl;
      free((void*)blocks[0].read_buf_);
      //std::cout << "After " << std::endl;
    };
  }

  if (blocks.size() == 2) {
    when(blocks[0].ptr_,blocks[1].ptr_) << [=](acquired_cown<Block> blk0,acquired_cown<Block> blk1) {
      blk0->write(blocks[0].read_buf_,blocks[0].bytes_to_write_,blocks[0].block_offset_,blocks[0].bytes_written_);
      blk1->write(blocks[1].read_buf_,blocks[1].bytes_to_write_,blocks[1].block_offset_,blocks[1].bytes_written_);
      //std::cout << "Freeing memory2: " << static_cast<const void *>(blocks[1].read_buf_) << std::endl;
      free((void*)blocks[1].read_buf_);
      //std::cout << " After " << std::endl;
    };
  }



  // Add more conditions for handling additional number of bodies if needed
}

void RegInode::write(const char* buf, size_t size, off_t offset,fuse_req_t req,std::atomic<size_t >&avail_bytes, int *ptr)
{

  //if (count ==1)
  //  printf("aaaaaa\n");

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
  u_int64_t total = remaining_size;

  //std::cout << "Size is " << remaining_size << std::endl;

  std::vector<Wrapper> vec_array;

  while (remaining_size>0)
  {
    //printf("bytes written: %ud\n",bytes_written);
    blockId = (offset + bytes_written) / BLOCK_SIZE;
    block_offset = (offset + bytes_written) % BLOCK_SIZE;

    if (data_blocks.find(blockId) == data_blocks.end())
    {
      data_blocks.emplace(blockId, make_cown<Block>(BLOCK_SIZE,blockId));
      //printf("block id %lu -----> new_Block\n",blockId);

      int ret = allocate_space(blockId,avail_bytes);

      //if (ret){
      //  fuse_reply_err(req,-ret);
      //  exit(-99);
      //}

      i_st.st_blocks = std::min(i_st.st_blocks+1,(__blkcnt_t)data_blocks.size());
    }

    size_t bytes_to_write = std::min(BLOCK_SIZE - block_offset, remaining_size);
    cown_ptr<Block> blk = data_blocks.at(blockId);

    Wrapper wrapper(blk,bytes_to_write,block_offset,bytes_written,size,buf);
    vec_array.push_back(wrapper);


    bytes_written += bytes_to_write;
    remaining_size -= bytes_to_write;
  }
  batch_write(vec_array);
}





//FIXME: What if someone does lseek(fd,1000,SEEK_CUR) --> write(fd,"a",1), and someone tries to read from offset 0? what will it return
int RegInode::read( size_t size, off_t offset, fuse_req_t req)
{
  auto init_req_mem = req->buf_mem;
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





class DirInode : public Inode {
public:
  //using name = type;
  using dir_t = std::map<std::string, uint64_t>  ;

  DirInode(
    fuse_ino_t &ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    mode_t mode,
    FileSystem* fs)
  : Inode(ino==1? 1: reinterpret_cast<fuse_ino_t >(this), time, uid, gid, blksize, fs) {
    i_st.st_nlink = 2;
    i_st.st_blocks = 1;
    i_st.st_mode = S_IFDIR | mode;
    ino = reinterpret_cast<fuse_ino_t >(this);
  }

  // Default constructor
  DirInode() : Inode(-1, -1, -1, -1, -1, nullptr) {
    // Initialize other members if needed
    i_st.st_nlink  = -1;
    i_st.st_blocks = -1;
    i_st.st_mode   = -1;
    fake = true;
  }

  bool fake = false;

  ~DirInode();

  dir_t dentries;


};


bool Inode::is_regular() const { return i_st.st_mode & S_IFREG; }

//bool Inode::is_directory() const { return i_st.st_mode & S_IFDIR; }

//bool Inode::is_symlink() const { return i_st.st_mode & S_IFLNK; }


Inode::~Inode() {
  std::cout << "hey" << std::endl;
}

RegInode::~RegInode() {
  std::cout << "hey" << std::endl;
};

DirInode::~DirInode() {
  std::cout << "hey" << std::endl;
};

struct FileHandle {
  cown_ptr<RegInode> in;
  int flags;

  FileHandle(cown_ptr<RegInode> in, int flags)
  : in(in)
    , flags(flags) {}


};



class FileSystem : public filesystem_base{
public:
  explicit FileSystem(size_t size);
  FileSystem(const FileSystem& other) = delete;
  FileSystem(FileSystem&& other) = delete;
  ~FileSystem() = default;
  FileSystem& operator=(const FileSystem& other) = delete;
  FileSystem& operator=(const FileSystem&& other) = delete;


  const fuse_lowlevel_ops& ops() const {
    return ops_;
  }

  // Fuse operations
public:






  // file handle operation
public:
  int create(fuse_ino_t parent_ino,const std::string& name,mode_t mode,int flags,uid_t uid,gid_t gid, fuse_req_t req,struct fuse_file_info* fi) override;
  int getattr(fuse_ino_t ino,  uid_t uid, gid_t gid, fuse_req_t req, int *ptr) override;
  ssize_t readdir(fuse_req_t req, fuse_ino_t ino,  size_t bufsize, off_t off) override;
  int mkdir(fuse_ino_t parent_ino,const std::string& name,mode_t mode, uid_t uid,gid_t gid, fuse_req_t req) override;
  //int mknod(fuse_ino_t parent_ino,const std::string& name,mode_t mode,dev_t rdev,struct stat* st,uid_t uid,gid_t gid);
  int access(fuse_ino_t ino, int mask, uid_t uid, gid_t gid,fuse_req_t req) override;
  int setattr(fuse_ino_t ino,FileHandle* fh,struct stat* x,int to_set,uid_t uid,gid_t gid,fuse_req_t req) override;
  int lookup(fuse_ino_t parent_ino, const std::string& name,fuse_req_t req) override;
  void init(void *userdata,struct fuse_conn_info *conn) override;
  int open(fuse_ino_t ino, int flags, FileHandle** fhp, uid_t uid, gid_t gid,  struct fuse_file_info* fi,fuse_req_t req) override;
  ssize_t write(FileHandle* fh, const char * buf, size_t size, off_t off, struct fuse_file_info *fi,fuse_req_t req,int * ptr, fuse_ino_t ino) override;
  ssize_t read(FileHandle* fh, off_t offset, size_t size,fuse_req_t req, fuse_ino_t ino) override;
  int unlink(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid,fuse_req_t req) override;
  int rmdir(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid,fuse_req_t req) override;
  void forget(fuse_ino_t ino, long unsigned nlookup,fuse_req_t req) override;
  int rename(fuse_ino_t acq_old_parent_in,const std::string& oldname,fuse_ino_t newparent_ino,const std::string& dupl_reg_in,uid_t uid,gid_t dupl_dir_in,fuse_req_t req) override;
  int opendir(fuse_ino_t ino, int flags, uid_t uid, gid_t gid,fuse_req_t req,struct fuse_file_info *fi) override;
  void release(fuse_ino_t ino, FileHandle* fh) override;
  void my_fallocate(fuse_req_t req, fuse_ino_t ino, int mode,off_t offset, off_t length, fuse_file_info *fi) override;
  // helper functions
private:
  void replace_entry_same_parent(uint64_t  uid,cown_ptr<DirInode> old_parent_inode,acquired_cown<DirInode> &acq_old_parent_in,std::string newname, std::map<std::basic_string<char>, uint64_t>::iterator old_entry,fuse_req_t req );
  void replace_entry(cown_ptr<DirInode> old_parent_inode,cown_ptr<DirInode> new_parent_inode,uint64_t  uid,acquired_cown<DirInode> &acq_old_parent_in,acquired_cown<DirInode> &acq_new_parent_in,std::string newname,std::map<std::basic_string<char>, uint64_t>::iterator old_entry,fuse_req_t req);
  void free_space(acquired_cown<Block> &blk);
  int truncate(acquired_cown<RegInode> &in, off_t newsize, uid_t uid, gid_t gid);

  void add_inode(cown_ptr<DirInode> inode,struct stat st);
  void add_inode(cown_ptr<RegInode> inode,struct stat st);

  static void get_inode(const cown_ptr<RegInode>& inode_cown, acquired_cown<RegInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>> &regular_inode_table);
  static void get_inode(const cown_ptr<DirInode>& in_cown, acquired_cown<DirInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>> &dir_inode_table);



  // private fields;
  cown_ptr<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>> regular_inode_table = make_cown<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>>();
  cown_ptr<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>> dir_inode_table = make_cown<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>>();

private:
  int access(struct stat st, int mask, uid_t uid, gid_t gid);
  static void init_stat(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode,struct stat *i_st,bool is_regular);

private:
  std::atomic<fuse_ino_t> next_ino_;
  //FIXME, SHOULD BE ATOMIC
  std::atomic<size_t> avail_bytes_;
  struct statvfs stat;
  cown_ptr<DirInode> root;

};



void FileSystem::init_stat(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode,struct stat *i_st,bool is_regular)
{
  memset(i_st, 0, sizeof(struct stat));
  i_st->st_ino = ino;
  i_st->st_atime = time;
  i_st->st_mtime = time;
  i_st->st_ctime = time;
  i_st->st_uid = uid;
  i_st->st_gid = gid;
  i_st->st_blksize = blksize;
  //Reg
  if(is_regular){
    i_st->st_nlink = 1;
    i_st->st_mode = S_IFREG | mode;
  }else{
    i_st->st_nlink = 2;
    i_st->st_blocks = 1;
    i_st->st_mode = S_IFDIR | mode;
  }

}


void FileSystem::add_inode(cown_ptr<DirInode> ino,struct stat st)
{
  when(ino,dir_inode_table) << [=](acquired_cown<DirInode> inode,auto table){
    assert(inode->krefs == 0);
    inode->i_st = st;
    inode->ino = st.st_ino;
    inode->krefs++;
    [[maybe_unused]] auto res = table->emplace(inode->ino, ino);
  };
}

void FileSystem::add_inode(cown_ptr<RegInode> ino,struct stat st)
{
  when(ino,regular_inode_table) << [=](acquired_cown<RegInode> inode,auto table){
    assert(inode->krefs == 0);
    inode->i_st = st;
    inode->ino = st.st_ino;
    inode->krefs++;
    [[maybe_unused]] auto res = table->emplace(inode->ino, ino);
  };
}

void FileSystem::get_inode(const cown_ptr<RegInode>& inode_cown, acquired_cown<RegInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>> &regular_inode_table)
{
  acq_inode->krefs++;
  auto res = regular_inode_table->emplace(acq_inode->ino, inode_cown);
  if (!res.second) {
    assert(acq_inode->krefs > 0);
  } else {
    assert(acq_inode->krefs == 0);
  }
}

void FileSystem::get_inode(const cown_ptr<DirInode>& in_cown, acquired_cown<DirInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>> &dir_inode_table)
{
  acq_inode->krefs++;
  auto res = dir_inode_table->emplace(acq_inode->ino, in_cown);
  if (!res.second) {
    assert(acq_inode->krefs > 0);
  } else {
    assert(acq_inode->krefs == 0);
  }
}


FileSystem::FileSystem(size_t size): next_ino_(FUSE_ROOT_ID) {
  auto now = std::time(nullptr);

  auto node_id = next_ino_++;
  //printf("node id is :%lu\n",node_id);

  root = make_cown<DirInode>(node_id, now, getuid(), getgid(), BLOCK_SIZE, 0755, this);

  avail_bytes_ = size;

  memset(&stat, 0, sizeof(stat));
  stat.f_fsid = 983983;
  stat.f_namemax = PATH_MAX;
  stat.f_bsize = BLOCK_SIZE;
  stat.f_frsize = BLOCK_SIZE;
  stat.f_blocks = size / BLOCK_SIZE;
  stat.f_files = 0;
  stat.f_bfree = stat.f_blocks;
  stat.f_bavail = stat.f_blocks;

  if (size < 1ULL << 20) {
    printf("creating %zu byte file system\n", size);
  } else if (size < 1ULL << 30) {
    auto mbs = size / (1ULL << 20);
    printf("creating %llu mb file system", mbs);
  } else if (size < 1ULL << 40) {
    auto gbs = size / (1ULL << 30);
    printf("creating %llu gb file system", gbs);
  } else if (size < 1ULL << 50) {
    auto tbs = size / (1ULL << 40);
    printf("creating %llu tb file system", tbs);
  } else {

  }
}


void FileSystem::init(void* userdata, struct fuse_conn_info* conn)
{
  //auto node_id = reinterpret_cast<uint64_t > (root.allocated_cown);

  struct stat i_st{};
  init_stat(1, std::time(nullptr), getuid(), getgid(), BLOCK_SIZE, 0755,&i_st, false);
  add_inode(root,i_st);
}

int FileSystem::lookup(fuse_ino_t parent_ino, const std::string& name, fuse_req_t req)
{

  std::cout << "in lookup for ino " << parent_ino << std::endl;
  //while(1);
  cown_ptr<DirInode> parent_in = nullptr;
  if(parent_ino == 1)
    parent_in = root;
  else{
    auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
    it->acquire_strong_from_weak();
    parent_in = cown_ptr<DirInode>(it);
  }

  when(parent_in) << [=](auto  parent_in){
    auto iter = parent_in->dentries.find(name);
    if (iter == parent_in->dentries.end()) {
      //log_->debug("lookup parent {} name {} not found", parent_ino, name);
      reply_fail(-ENOENT, req);
      return -ENOENT;
    }

    auto in = iter->second;




    // check inode is regular inode
    if(in%2==1){
      in--;
      auto it = reinterpret_cast<ActualCown<RegInode> *>(in);
      it->acquire_strong_from_weak();
      cown_ptr<RegInode> inode = cown_ptr<RegInode>(it);
      when(inode,regular_inode_table) << [=](acquired_cown<RegInode> acq_inode,auto  reg_table){
        get_inode(inode,acq_inode,reg_table);
        struct fuse_entry_param fe{};
        std::memset(&fe, 0, sizeof(struct fuse_entry_param));
        fe.attr = acq_inode->i_st;
        fe.attr_timeout = 86400.0;
        fe.entry_timeout = 86400.0;
        reply_lookup_success(fe,req);
        return 0;
      };
    }else{
      cown_ptr<DirInode> inode = nullptr;
      if(in == 1)
        inode = root;
      else{
        auto it = reinterpret_cast<ActualCown<DirInode> *>(in);
        it->acquire_strong_from_weak();
        inode = cown_ptr<DirInode>(it);
      }
      when(inode,dir_inode_table) << [=](acquired_cown<DirInode> acq_inode,auto  dir_table){
        get_inode(inode,acq_inode,dir_table);
        struct fuse_entry_param fe{};
        std::memset(&fe, 0, sizeof(fe));
        fe.attr = acq_inode->i_st;
        fe.attr_timeout = 86400.0;
        fe.entry_timeout = 86400.0;
        reply_lookup_success(fe,req);
        return 0;
      };
    }
    return 0;
  };
  return 0;
}

void FileSystem::release(fuse_ino_t ino, FileHandle* fh)
{
  delete fh;
}

int FileSystem::access(struct stat i_st, int mask, uid_t uid, gid_t gid) {
  if (mask == F_OK) return 0;

  assert(mask & (R_OK | W_OK | X_OK));

  if (i_st.st_uid == uid) {
    if (mask & R_OK) {
      if (!(i_st.st_mode & S_IRUSR)) return -EACCES;
    }
    if (mask & W_OK) {
      if (!(i_st.st_mode & S_IWUSR)) return -EACCES;
    }
    if (mask & X_OK) {
      if (!(i_st.st_mode & S_IXUSR)) return -EACCES;
    }
    return 0;
  } else if (i_st.st_gid == gid) {
    if (mask & R_OK) {
      if (!(i_st.st_mode & S_IRGRP)) return -EACCES;
    }
    if (mask & W_OK) {
      if (!(i_st.st_mode & S_IWGRP)) return -EACCES;
    }
    if (mask & X_OK) {
      if (!(i_st.st_mode & S_IXGRP)) return -EACCES;
    }
    return 0;
  } else if (uid == 0) {
    if (mask & X_OK) {
      if (!(i_st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        return -EACCES;
    }
    return 0;
  } else {
    if (mask & R_OK) {
      if (!(i_st.st_mode & S_IROTH)) return -EACCES;
    }
    if (mask & W_OK) {
      if (!(i_st.st_mode & S_IWOTH)) return -EACCES;
    }
    if (mask & X_OK) {
      if (!(i_st.st_mode & S_IXOTH)) return -EACCES;
    }
    return 0;
  }

  assert(0);
}

int FileSystem::create(fuse_ino_t parent_ino, const std::string& name, mode_t mode, int flags, uid_t uid, gid_t gid, fuse_req_t req, struct fuse_file_info* fi2)
{
  //fi->direct_io = 1;
  struct fuse_file_info fi = *fi2;
  //fi.parallel_direct_writes = 1;
  //fi.direct_io = 1;

  //printf("Setting direct writes");
  if(name.length() > NAME_MAX){
    std::cout << "name: " << name << "is too long" << std::endl;
    reply_fail(-ENAMETOOLONG, req);
    return -ENAMETOOLONG;
  }

  auto now = std::time(nullptr);

  cown_ptr<DirInode> parent_in = nullptr;
  if(parent_ino==1)
    parent_in = root;
  else
  {
    auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
    it->acquire_strong_from_weak();
    parent_in = cown_ptr<DirInode>(it);
  }

  when(parent_in) << [=](acquired_cown<DirInode> parent_in){


    fuse_ino_t  node_id;
    cown_ptr<RegInode> in = make_cown<RegInode>(node_id, now, uid, gid, BLOCK_SIZE, S_IFREG | mode, this);
    node_id =  reinterpret_cast<uint64_t>(in.allocated_cown) + 1;
    struct stat i_st{};
    init_stat(node_id,now,uid,gid,BLOCK_SIZE,S_IFREG | mode,&i_st, true);

    auto fh = std::make_unique<FileHandle>(in, flags);
    DirInode::dir_t& children = parent_in->dentries;

    if (children.find(name) != children.end()) {
      std::cout << "create name" << name << " already exists" << std::endl;
      reply_fail(-EEXIST, req);
      //exit(-99);
      return -EEXIST;
    }

    int ret = access(parent_in->i_st, W_OK, uid, gid);
    if (ret) {
      //log_->debug("create name {} access denied ret {}", name, ret);
      reply_fail(ret, req);
      //exit(-99);
      return ret;
    }

    //printf("Reg Inode is %lu\n",node_id);
    children[name]= reinterpret_cast<uint64_t >(node_id) ;
    add_inode(in,i_st);

    parent_in->i_st.st_ctime = now;
    parent_in->i_st.st_mtime = now;

    //printf("Direct writes are %d",fi.parallel_direct_writes);

    struct fuse_file_info copy = fi;
    copy.fh = reinterpret_cast<uint64_t>(fh.release());

    reply_create_success(&copy,NULL,req,i_st);
    return 0;
  };
  return 0;
}

ssize_t FileSystem::readdir(fuse_req_t req, fuse_ino_t parent_ino, size_t bufsize, off_t toff)
{
  // Again same pattern, first acquire dir_inode_table
  // then dir inode and then iterate through its entries

  size_t off = toff;
  char *buf = new char[bufsize];


  size_t pos = 0;

  /*
   * FIXME: the ".." directory correctly shows up at the parent directory
   * inode, but "." shows a inode number as "?" with ls -lia.
   */
  if (off == 0) {
    size_t remaining = bufsize - pos;
    struct stat st{};
    memset(&st, 0, sizeof(struct stat));
    st.st_ino = 1;
    size_t used = fuse_add_direntry(req, buf + pos, remaining, ".", &st, 1);
    if (used > remaining){
      fuse_reply_buf(req, buf, pos);
      free(buf);
      return pos;
    }
    //printf("Str %s",buf+pos);
    pos += used;
    off = 1;
  }



  if (off == 1) {
    size_t remaining = bufsize - pos;
    struct stat st{};
    memset(&st, 0, sizeof(struct stat));
    st.st_ino = 1;
    size_t used = fuse_add_direntry(
      req, buf + pos, remaining, "..", &st, 2);
    if (used > remaining) {
      fuse_reply_buf(req, buf, pos);
      free(buf);
      return pos;
    }
    pos += used;
    off = 2;
  }


  assert(off >= 2);

  //std::cout << "ino is " << parent_ino << std::endl;

  cown_ptr<DirInode> parent_in = nullptr;
  if(parent_ino==1)
    parent_in = root;
  else
  {
    auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
    it->acquire_strong_from_weak();
    parent_in = cown_ptr<DirInode>(it);
  }
  when(parent_in) << [=](auto acq_parent_in){
    auto cp_pos = pos;
    auto cp_off = off;
    DirInode::dir_t& children = acq_parent_in->dentries;

    size_t count = 0;
    size_t target = cp_off - 2;

    for (auto & it : children) {
      if (count >= target) {
        auto in = it.second;
        struct stat st;
        memset(&st, 0, sizeof(struct stat ));
        st.st_ino = in;
        size_t remaining = bufsize - cp_pos;
        size_t used = fuse_add_direntry(
          req, buf + cp_pos, remaining, it.first.c_str(), &st, cp_off + 1);
        if (used > remaining){
          fuse_reply_buf(req, buf, (size_t)cp_pos);
          free(buf);
          return cp_pos;
        }
        cp_pos += used;
        cp_off++;
      }
      count++;
    }
    fuse_reply_buf(req, buf,cp_pos);
    free(buf);
    return (size_t)0;
  };
  return 0;
}




int FileSystem::getattr(fuse_ino_t ino, uid_t uid, gid_t gid, fuse_req_t req,int *ptr)
{
  //std::cout << "getattr: ino is " << ino << std::endl;

  if(ino==1 || ino%2==0){
    cown_ptr<DirInode> dirInode = nullptr;
    if(ino==1)
      dirInode = root;
    else
    {
      auto it = reinterpret_cast<ActualCown<DirInode> *>(ino);
      it->acquire_strong_from_weak();
      dirInode = cown_ptr<DirInode>(it);
    }
    when(dirInode) << [=](acquired_cown<DirInode> dirInode){
      //std::cout << "getattr2: ino is " << ino << std::endl;
      struct stat st = dirInode->i_st;
      fuse_reply_attr(req, &st, 86400.0);
    };
  }
  else if(ino%2==1){
    ino--;
    auto it = reinterpret_cast<ActualCown<RegInode> *>(ino);
    it->acquire_strong_from_weak();
    cown_ptr<RegInode> regInode = cown_ptr<RegInode>(it);
    when(regInode) << [=](acquired_cown<RegInode> reg_ino){
      struct stat st = reg_ino->i_st;
      fuse_reply_attr(req, &st, 86400.0);
    };
  }
  return 0;
}

int FileSystem::setattr(fuse_ino_t ino, FileHandle* fh, struct stat* x, int to_set, uid_t uid, gid_t gid, fuse_req_t req)
{

  //Case where it's a regular inode
  struct stat attr = *x;

  if(ino%2==1){
    auto allocated_cown = reinterpret_cast<ActualCown<RegInode>*>(ino-1);
    allocated_cown->acquire_strong_from_weak();
    cown_ptr<RegInode> reg_inode = cown_ptr<RegInode>(allocated_cown);
    when(reg_inode) << [=]( acquired_cown<RegInode> reg_inode){

      mode_t clear_mode = 0;
      auto now = std::time(nullptr);
      if (to_set & FUSE_SET_ATTR_MODE) {
        if (uid && reg_inode->i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && reg_inode->i_st.st_gid != gid) clear_mode |= S_ISGID;

        reg_inode->i_st.st_mode = attr.st_mode;
      }

      if (to_set & (FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
        /*
       * Only  a  privileged  process  (Linux: one with the CAP_CHOWN
       * capability) may change the owner of a file.  The owner of a file may
       * change the group of the file to any group of which that owner is a
       * member.  A privileged process (Linux: with CAP_CHOWN) may change the
       * group arbitrarily.
       *
       * TODO: group membership for owner is not enforced.
         */
        if (
          uid && (to_set & FUSE_SET_ATTR_UID)
          && (reg_inode->i_st.st_uid != attr.st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && (to_set & FUSE_SET_ATTR_GID) && (uid != reg_inode->i_st.st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (to_set & FUSE_SET_ATTR_UID) reg_inode->i_st.st_uid = attr.st_uid;

        if (to_set & FUSE_SET_ATTR_GID) reg_inode->i_st.st_gid = attr.st_gid;
      }

      if (to_set & (FUSE_SET_ATTR_MTIME | FUSE_SET_ATTR_ATIME)) {
        if (uid && reg_inode->i_st.st_uid != uid)
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

#ifdef FUSE_SET_ATTR_MTIME_NOW
        if (to_set & FUSE_SET_ATTR_MTIME_NOW)
          reg_inode->i_st.st_mtime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_MTIME)
          reg_inode->i_st.st_mtime = attr.st_mtime;

#ifdef FUSE_SET_ATTR_ATIME_NOW
        if (to_set & FUSE_SET_ATTR_ATIME_NOW)
          reg_inode->i_st.st_atime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_ATIME)
          reg_inode->i_st.st_atime = attr.st_atime;
      }

#ifdef FUSE_SET_ATTR_CTIME
      if (to_set & FUSE_SET_ATTR_CTIME) {
        if (uid && reg_inode->i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }
        reg_inode->i_st.st_ctime = attr.st_ctime;
      }
#endif

      if (to_set & FUSE_SET_ATTR_SIZE) {
        if (uid) {     // not root
          if (!fh) { // not open file descriptor
            int ret = access(reg_inode->i_st, W_OK, uid, gid);
            if (ret) {
              fuse_reply_err(req, -ret);
              return ret;
            }
          } else if (
            ((fh->flags & O_ACCMODE) != O_WRONLY)
            && ((fh->flags & O_ACCMODE) != O_RDWR)) {
            reply_fail(-EACCES,req);
            return -EACCES;
          }
        }

        // impose maximum size of 2TB
        if (attr.st_size > 2199023255552) {
          reply_fail(-EFBIG,req);
          return -EFBIG;
        }

        assert(reg_inode->is_regular());



        int ret = truncate(reg_inode, attr.st_size, uid, gid);
        if (ret < 0) {
          reply_fail(ret,req);
          return ret;
        }

        reg_inode->i_st.st_mtime = now;
      }
      reg_inode->i_st.st_ctime = now;
      if (to_set & FUSE_SET_ATTR_MODE) reg_inode->i_st.st_mode &= ~clear_mode;

      //*attr = i_st;
      fuse_reply_attr(req, &reg_inode->i_st, 0);
      return 0;
    };
  }

  //Case where it's a dir inode
  if(ino==1 || ino%2==0){
    auto it = reinterpret_cast<ActualCown<DirInode> *>(ino);
    it->acquire_strong_from_weak();
    cown_ptr<DirInode> dir_inode = cown_ptr<DirInode>(it);


    when(dir_inode,dir_inode_table) << [=]( acquired_cown<DirInode> reg_inode,auto ){

      mode_t clear_mode = 0;

      auto now = std::time(nullptr);
      if (to_set & FUSE_SET_ATTR_MODE) {
        if (uid && reg_inode->i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && reg_inode->i_st.st_gid != gid) clear_mode |= S_ISGID;

        reg_inode->i_st.st_mode = attr.st_mode;
      }

      if (to_set & (FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
        /*
         * Only  a  privileged  process  (Linux: one with the CAP_CHOWN
         * capability) may change the owner of a file.  The owner of a file may
         * change the group of the file to any group of which that owner is a
         * member.  A privileged process (Linux: with CAP_CHOWN) may change the
         * group arbitrarily.
         *
         * TODO: group membership for owner is not enforced.
         */
        if (
          uid && (to_set & FUSE_SET_ATTR_UID)
          && (reg_inode->i_st.st_uid != attr.st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && (to_set & FUSE_SET_ATTR_GID) && (uid != reg_inode->i_st.st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (to_set & FUSE_SET_ATTR_UID) reg_inode->i_st.st_uid = attr.st_uid;

        if (to_set & FUSE_SET_ATTR_GID) reg_inode->i_st.st_gid = attr.st_gid;
      }

      if (to_set & (FUSE_SET_ATTR_MTIME | FUSE_SET_ATTR_ATIME)) {
        if (uid && reg_inode->i_st.st_uid != uid)
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

#ifdef FUSE_SET_ATTR_MTIME_NOW
        if (to_set & FUSE_SET_ATTR_MTIME_NOW)
          reg_inode->i_st.st_mtime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_MTIME)
          reg_inode->i_st.st_mtime = attr.st_mtime;

#ifdef FUSE_SET_ATTR_ATIME_NOW
        if (to_set & FUSE_SET_ATTR_ATIME_NOW)
          reg_inode->i_st.st_atime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_ATIME)
          reg_inode->i_st.st_atime = attr.st_atime;
      }

#ifdef FUSE_SET_ATTR_CTIME
      if (to_set & FUSE_SET_ATTR_CTIME) {
        if (uid && reg_inode->i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }
        reg_inode->i_st.st_ctime = attr.st_ctime;
      }
#endif

      if (to_set & FUSE_SET_ATTR_SIZE) {
        if (uid) {     // not root
          if (!fh) { // not open file descriptor
            int ret = access(reg_inode->i_st, W_OK, uid, gid);
            if (ret) {
              fuse_reply_err(req, -ret);
              return ret;
            }
          } else if (
            ((fh->flags & O_ACCMODE) != O_WRONLY)
            && ((fh->flags & O_ACCMODE) != O_RDWR)) {
            reply_fail(-EACCES,req);
            return -EACCES;
          }
        }

        // impose maximum size of 2TB
        if (attr.st_size > 2199023255552) {
          reply_fail(-EFBIG,req);
          return -EFBIG;
        }

        assert(reg_inode->is_regular());


        //TODO implement truncate
        //auto reg_in = std::dynamic_pointer_cast<RegInode>(in);
        //int ret = truncate(reg_in, attr->st_size, uid, gid);
        int ret  = 0;
        if (ret < 0) {
          return ret;
        }

        reg_inode->i_st.st_mtime = now;
      }
      reg_inode->i_st.st_ctime = now;
      if (to_set & FUSE_SET_ATTR_MODE) reg_inode->i_st.st_mode &= ~clear_mode;


      fuse_reply_attr(req, &reg_inode->i_st, 0);
      return 0;
    };
  };
  return 0;
}
// virtual int opendir(fuse_ino_t ino, int flags, uid_t uid, gid_t gid,fuse_req_t req,struct fuse_file_info *fi)
int FileSystem::opendir(fuse_ino_t ino, int flags, uid_t uid, gid_t gid, fuse_req_t req,struct fuse_file_info *fi)
{
  if(ino==1 || ino%2==0){
    cown_ptr<DirInode> in = nullptr;
    if(ino==1)
      in = root;
    else
    {
      auto it = reinterpret_cast<ActualCown<DirInode> *>(ino);
      it->acquire_strong_from_weak();
      in = cown_ptr<DirInode>(it);
    }
    when(in) << [=](auto dir_in){
      if ((flags & O_ACCMODE) == O_RDONLY) {
        int ret = access(dir_in->i_st, R_OK, uid, gid);
        if (ret) {
          reply_fail(ret,req);
          return ret;
        }
      }
      //printf("Direct writes var is %d\n",fi->parallel_direct_writes);

      fuse_reply_open(req,fi);
      return 0;
    };
  }
  else{
    ino--;
    auto it = reinterpret_cast<ActualCown<RegInode> *>(ino);
    it->acquire_strong_from_weak();
    cown_ptr<RegInode> regInode = cown_ptr<RegInode>(it);
    when(regInode) << [=](auto reg_in) {
      if ((flags & O_ACCMODE) == O_RDONLY)
      {
        int ret = access(reg_in->i_st, R_OK, uid, gid);
        if (ret)
          return ret;
      }
      fuse_reply_open(req,fi);
      return 0;
    };
  }

  return 0;
}


int FileSystem::access(fuse_ino_t ino, int mask, uid_t uid, gid_t gid, fuse_req_t req)
{
  std::cout << " in access" << std::endl;
  if(ino==1 || ino%2==0){
    std::cout << " in if access" << std::endl;
    cown_ptr<DirInode> dir_inode = nullptr;
    if(ino==1){
      dir_inode = root;
    }else{
      auto it = reinterpret_cast<ActualCown<DirInode> *>(ino);
      it->acquire_strong_from_weak();
      dir_inode = cown_ptr<DirInode>(it);
    }
    when(dir_inode) << [=](auto dir_inode){
      int ret = access(dir_inode->i_st, mask, uid, gid);
      std::cout << "replying kernel" << std::endl;
      fuse_reply_err(req, -ret);
    };
  }else{
    std::cout << " in else access" << std::endl;
    ino--;
    auto allocated_cown = reinterpret_cast<ActualCown<RegInode>*>(ino);
    allocated_cown->acquire_strong_from_weak();
    cown_ptr<RegInode> reg_inode = cown_ptr<RegInode>(allocated_cown);
    when(reg_inode) << [=]( auto reg_inode){
      int ret = access(reg_inode->i_st, mask, uid, gid);
      fuse_reply_err(req, -ret);
    };
  }
  return 0;
}

int FileSystem::mkdir(fuse_ino_t parent_ino, const std::string& name, mode_t mode, uid_t uid, gid_t gid, fuse_req_t req)
{
  if (name.length() > NAME_MAX) {
    const int ret = -ENAMETOOLONG;
    reply_fail(ret,req);
    return ret;
  }
  auto now = std::time(nullptr);
  // First acquires directory_inode_table which is a cown
  // then acquire parent DirInode which is also a cown and adds an entry
  // also add inode to dir inode table

  cown_ptr<DirInode> parent_in = nullptr;
  if(parent_ino==1){
    parent_in = root;
  }else{
    auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
    it->acquire_strong_from_weak();
    parent_in = cown_ptr<DirInode>(it);
  }

  when(parent_in) << [=](auto parent_in){
    DirInode::dir_t& children = parent_in->dentries;
    if (children.find(name) != children.end()) {
      const int ret = -EEXIST;
      reply_fail(ret,req);
      return ret;
    }
    int ret = access(parent_in->i_st, W_OK, uid, gid);
    if (ret) {
      reply_fail(ret,req);
      return ret;
    }

    fuse_ino_t node_id = 0;
    struct stat i_st{};
    cown_ptr<DirInode> in = make_cown<DirInode>(node_id, now, uid, gid, BLOCK_SIZE, mode, this);
    node_id = reinterpret_cast<uint64_t > (in.allocated_cown);
    init_stat(node_id, now, uid, gid, BLOCK_SIZE, mode,&i_st, false);
    printf("Dir Inode is %lu\n",node_id);
    children[name] = node_id;
    add_inode(in,i_st);

    parent_in->i_st.st_ctime = now;
    parent_in->i_st.st_mtime = now;
    parent_in->i_st.st_nlink++;

    reply_mkdir(i_st,req);
    return 0;
  };
  return 0;
}

int FileSystem::open(fuse_ino_t ino, int flags, FileHandle** fhp, uid_t uid, gid_t gid, struct fuse_file_info* fi2,fuse_req_t req)
{
  struct fuse_file_info fi = *fi2;
  //fi.parallel_direct_writes =1;
  //fi.direct_io = 1;

  //printf("Setting direct writes");
  int mode = 0;
  if ((flags & O_ACCMODE) == O_RDONLY)
    mode = R_OK;
  else if ((flags & O_ACCMODE) == O_WRONLY)
    mode = W_OK;
  else if ((flags & O_ACCMODE) == O_RDWR)
    mode = R_OK | W_OK;

  if (!(mode & W_OK) && (flags & O_TRUNC)) {
    const int ret = EACCES;
    fuse_reply_err(req,ret);
    return ret;
  }

  ino--;
  auto allocated_cown = reinterpret_cast<ActualCown<RegInode>*>(ino);
  allocated_cown->acquire_strong_from_weak();
  cown_ptr<RegInode> reg_inode = cown_ptr<RegInode>(allocated_cown);

  when(reg_inode) << [=](acquired_cown<RegInode> acquiredCown){

    auto fh = std::make_unique<FileHandle>(reg_inode, flags);
    int ret = access(acquiredCown->i_st, mode, uid, gid);

    if (ret) {
      reply_fail(ret,req);
      return ret;
    }


    if (flags & O_TRUNC) {
      ret = truncate(acquiredCown, 0, uid, gid);
      if (ret) {
        reply_fail(ret,req);
        return ret;
      }
      auto now = std::time(nullptr);
      acquiredCown->i_st.st_mtime = now;
      acquiredCown->i_st.st_ctime = now;
    }


    printf("Direct writes var is %d\n",fi.parallel_direct_writes);

    fuse_reply_open(req, &fi);
    return 0;
  };
  return 0;
}

struct WriteData {
  char *buf;
  size_t size;
  off_t offset;
  std::atomic<size_t>& avail_bytes;
};




std::vector<WriteData> accumulated_writes;  // Accumulated write

void applyBatchWrites(fuse_ino_t ino){


  std::vector<WriteData> copy = accumulated_writes;

  ino--;
  auto allocated_cown = reinterpret_cast<ActualCown<RegInode>*>(ino);
  allocated_cown->acquire_strong_from_weak();
  cown_ptr<RegInode> reg_inode = cown_ptr<RegInode>(allocated_cown);

  when(reg_inode) << [=](auto regInode) {
    //std::cout << "Before " << std::endl;

    for (const WriteData &write_data : copy) {
      regInode->write(write_data.buf, write_data.size, write_data.offset, NULL, write_data.avail_bytes, NULL);
    }
    //std::cout << "After " << std::endl;
  };
  count = 0;
  accumulated_writes.clear();
}

//TODO: Find a way to make writes be written eventually, and also solve the issue with reads
ssize_t FileSystem::write(FileHandle* fh, const char* buf, size_t size, off_t off, struct fuse_file_info* fi, fuse_req_t req, int *ptr,fuse_ino_t ino)
{


  char *copy = static_cast<char*>(malloc(size));
  memcpy(copy,buf,size);

  accumulated_writes.push_back({copy, size, off,  avail_bytes_});
  count++;
  //std::cout << "count: " << count << " and thread " << pthread_self() << std::endl;
  // Check if the count reaches the threshold
  if(count==100)
  {
    //std::cout << "in if count: " << count << " and thread " << pthread_self() << std::endl;
    applyBatchWrites(ino);
  }
  return 0;
}

ssize_t FileSystem::read(FileHandle* fh, off_t offset, size_t size, fuse_req_t req, fuse_ino_t ino)
{

  applyBatchWrites(ino);
  ino--;
  auto allocated_cown = reinterpret_cast<ActualCown<RegInode>*>(ino);
  allocated_cown->acquire_strong_from_weak();
  cown_ptr<RegInode> reg_inode = cown_ptr<RegInode>(allocated_cown);
  when(reg_inode) << [=](acquired_cown<RegInode> reg_in){
    reg_in->read(size,offset,req);
  };
  return 0;
}

void FileSystem::free_space(acquired_cown<Block> & blk)
{
  printf("Deallocating block with id:%d\n",blk->block_number);
  blk->buf.release();
  //avail_bytes_+= BLOCK_SIZE;
  return ;
}


int FileSystem::truncate(acquired_cown<RegInode> &in, off_t newsize, uid_t uid, gid_t gid)
{

  if (in->i_st.st_size == newsize)
    return 0;

  if (newsize==0) {
    printf("newsize = 0\n");
    for(auto &pair: in->data_blocks){
      cown_ptr<Block> block = pair.second;
      when(block) << [=](acquired_cown<Block> blk){
        free_space(blk);
      };
    }
    in->data_blocks.clear();
    in->i_st.st_size = 0;
  }
  else if (newsize < in->i_st.st_size) {
    auto block_id = newsize / BLOCK_SIZE;
    auto it = in->data_blocks.upper_bound(block_id);
    while (it != in->data_blocks.end()){

      cown_ptr<Block> block = it->second;
      when(block) << [this](acquired_cown<Block> blk){
        free_space(blk);
      };

      it = std::next(it);
    }


    in->data_blocks.erase(it,in->data_blocks.end());
    in->i_st.st_size = newsize;
  }
  else{
    assert(in->i_st.st_size < newsize);
    // Allocate space for no existing blocks and init them with zero
    for(int i=0;i<newsize;i+=BLOCK_SIZE){
      int block_id = i/BLOCK_SIZE;
      if(in->data_blocks.find(block_id) == in->data_blocks.end())
      {
        printf("Allocating block %d\n",block_id);
        in->allocate_space(block_id,avail_bytes_);
      }
      in->i_st.st_size = newsize;
    }

    //printf("\n in truncate\n");
    //exit(-99);
  }

  return 0;
}

int FileSystem::unlink(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid,fuse_req_t req)
{
  cown_ptr<DirInode> parent_in = nullptr;
  if(parent_ino==1)
    parent_in = root;
  else
  {
    auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
    it->acquire_strong_from_weak();
    parent_in = cown_ptr<DirInode>(it);
  }
  when(parent_in) << [=](acquired_cown<DirInode> acq_parent_in) {

    auto entry_to_delete = acq_parent_in->dentries.find(name);

    //Entry doesnt exist
    if (entry_to_delete == acq_parent_in->dentries.end()) {
      reply_fail(-ENOENT,req);
      return -ENOENT;
    };

    // no access rights
    int ret = access(acq_parent_in->i_st, W_OK, uid, gid);
    if (ret) {
      reply_fail(-ret,req);
      return ret;
    }
    // if entry is a directory
    //if(dir_table->find(entry_to_delete->second) != dir_table->end()) {
    if(entry_to_delete->second % 2 ==0) {
      reply_fail(ret,req);
      return -EPERM;
    }

    //entry is regular file
    assert( entry_to_delete->second % 2 !=0 );

    auto now = std::time(nullptr);

    auto actual_cown = reinterpret_cast<ActualCown<RegInode> *>(entry_to_delete->second-1);
    actual_cown->acquire_strong_from_weak();
    cown_ptr<RegInode> reg_inode = cown_ptr<RegInode>(actual_cown);

    acq_parent_in->dentries.erase(entry_to_delete);
    when(reg_inode,parent_in) << [=](auto reg_in,auto parent_in){
      reg_in->i_st.st_ctime = now;
      reg_in->i_st.st_nlink--;

      parent_in->i_st.st_ctime = now;
      parent_in->i_st.st_mtime = now;
      fuse_reply_err(req, 0);
      return 0;
    };
    return 0;
  };
  return 0;
}

int FileSystem::rmdir(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid, fuse_req_t req)
{

  cown_ptr<DirInode> parent_in = nullptr;
  if(parent_ino==1)
    parent_in = root;
  else
  {
    auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
    it->acquire_strong_from_weak();
    parent_in = cown_ptr<DirInode>(it);
  }

  when(parent_in) << [=](auto acq_parent_in){
    DirInode::dir_t& children = acq_parent_in->dentries;
    auto entry_to_delete = children.find(name);
    if( entry_to_delete == children.end()){
      reply_fail(-ENOENT,req);
      return -ENOENT;
    }


    // if not a directory
    if(entry_to_delete->second!=1 && entry_to_delete->second%2!=0) {
      reply_fail(-ENOTDIR,req);
      return -ENOTDIR;
    }


    auto it = reinterpret_cast<ActualCown<DirInode> *>(entry_to_delete->second);
    it->acquire_strong_from_weak();
    cown_ptr<DirInode> inode_to_delete = cown_ptr<DirInode>(it);

    when(inode_to_delete,parent_in) << [req,entry_to_delete](auto inode_to_delete,auto parent_in){

      if(inode_to_delete->dentries.size()){
        reply_fail(-ENOTEMPTY,req);
        return -ENOTEMPTY;
      }

      inode_to_delete->i_st.st_nlink -= 2;
      if(inode_to_delete->i_st.st_nlink!=0)
        printf("aaaa");
      assert(inode_to_delete->i_st.st_nlink == 0);

      auto now = std::time(nullptr);
      parent_in->i_st.st_mtime = now;
      parent_in->i_st.st_ctime = now;
      parent_in->dentries.erase(entry_to_delete->first);
      parent_in->i_st.st_nlink--;
      fuse_reply_err(req, 0);
      return 0;
    };
    return 0;
  };
  return 0;
}

void FileSystem::my_fallocate(fuse_req_t req, fuse_ino_t ino, int mode, off_t offset, off_t length, fuse_file_info* fi)
{
  if (mode) {
    fuse_reply_err(req, EOPNOTSUPP);
    return;
  }

  ino--;

  auto it = reinterpret_cast<ActualCown<RegInode> *>(ino);
  it->acquire_strong_from_weak();
  cown_ptr<RegInode> regInode = cown_ptr<RegInode>(it);

  printf("offset %ld and length %ld for ino %lu\n",offset,length,ino);
  when(regInode) << [=](auto in){
    for(off_t i=offset;i<offset+length;i+=BLOCK_SIZE){
      off_t block_id = i/BLOCK_SIZE;
      if(in->data_blocks.find(block_id) == in->data_blocks.end())
        in->allocate_space(block_id,avail_bytes_);
    }
    in->i_st.st_size = std::max(in->i_st.st_size, offset+length);
    in->i_st.st_blocks = (__blkcnt_t)in->data_blocks.size();
    fuse_reply_err(req, 0);
  };
}

void FileSystem::replace_entry(cown_ptr<DirInode> old_parent_inode,cown_ptr<DirInode> new_parent_inode,uint64_t  uid,acquired_cown<DirInode> &acq_old_parent_in,acquired_cown<DirInode> &acq_new_parent_in,std::string newname,std::map<std::basic_string<char>, uint64_t>::iterator old_entry,fuse_req_t req){
  //new entry = (old_name,ino)
  auto dupl_entry =acq_new_parent_in->dentries.find(newname);


  //new_name does not exist as an entry to new_parent_entries -- easy, just remove the old entry (oldname,ino) and add new entry (newname,ino)
  if(dupl_entry == acq_new_parent_in->dentries.end()){
    //delete <old_name,ino> for old_parent_dir
    //std::cout << "Erasing entry" << old_entry->first << " " << old_entry->second << " from dir with ino " << acq_old_parent_in->ino << std::endl;
    acq_old_parent_in->dentries.erase(old_entry->first);
    //add entry <new_name,ino>
    acq_new_parent_in->dentries.insert({newname,old_entry->second});
    //reply to kernel

    if(old_entry->second%2==0)
    {
      acq_old_parent_in->i_st.st_nlink--;
      acq_new_parent_in->i_st.st_nlink++;
    }

    fuse_reply_err(req, 0);

  }
  // new_name already exists as an entry to new_parent_entries ==> decrement nlinks of this inode
  else{

    when(regular_inode_table,dir_inode_table) << [uid,old_entry,dupl_entry,old_parent_inode,new_parent_inode,req](auto reg_table,auto dir_table){
      cown_ptr<RegInode> old_reg_ino_cown = reg_table->find(old_entry->second) == reg_table->end() ? make_cown<RegInode>() : reg_table->find(old_entry->second)->second;
      cown_ptr<RegInode> dupl_reg_ino_cown = reg_table->find(dupl_entry->second) == reg_table->end() ? make_cown<RegInode>() : reg_table->find(dupl_entry->second)->second;

      cown_ptr<DirInode> old_dir_ino_cown = dir_table->find(old_entry->second) == dir_table->end() ? make_cown<DirInode>() : dir_table->find(old_entry->second)->second;
      cown_ptr<DirInode> dupl_dir_ino_cown = dir_table->find(dupl_entry->second) == dir_table->end() ? make_cown<DirInode>() : dir_table->find(dupl_entry->second)->second;


      when(old_parent_inode,
           new_parent_inode,
           old_reg_ino_cown,
           dupl_reg_ino_cown,
           old_dir_ino_cown,
           dupl_dir_ino_cown) << [=]
        (acquired_cown<DirInode> acq_old_parent_in,
         acquired_cown<DirInode>  acq_new_parent_in,
         acquired_cown<RegInode> old_reg_in,
         acquired_cown<RegInode> dupl_reg_in,
         acquired_cown<DirInode> old_dir_in,
         acquired_cown<DirInode> dupl_dir_in){

          if (
            !dupl_reg_in->fake && acq_new_parent_in->i_st.st_mode & S_ISVTX && uid
            && uid != dupl_reg_in->i_st.st_uid && uid != acq_new_parent_in->i_st.st_uid) {
            reply_fail(-EPERM,req);
            return -EPERM;
          }

          if (
            !dupl_dir_in->fake && acq_new_parent_in->i_st.st_mode & S_ISVTX && uid
            && uid != dupl_reg_in->i_st.st_uid && uid != acq_new_parent_in->i_st.st_uid) {
            reply_fail(-EPERM,req);
            return -EPERM;
          }


          if( !old_dir_in->fake && (old_dir_in->i_st.st_mode & S_IFDIR)){
            if( !dupl_dir_in->fake && (dupl_dir_in->i_st.st_mode & S_IFDIR)  ){
              if(!dupl_dir_in->dentries.empty()){
                reply_fail(-ENOTEMPTY,req);
                return -ENOTEMPTY;
              }
            }
            else{
              reply_fail(-ENOTDIR,req);
              return -ENOTDIR;
            }
          }else {
            if (!dupl_dir_in->fake && dupl_dir_in->i_st.st_mode & S_IFDIR) return -EISDIR;
          }
          acq_old_parent_in->i_st.st_ctime = std::time(nullptr);

          acq_old_parent_in->dentries.erase(old_entry->first);
          acq_new_parent_in->dentries.erase(dupl_entry->first);
          acq_new_parent_in->dentries.insert({old_entry->first,old_entry->second});

          if(!old_dir_in->fake && !dupl_dir_in->fake){
            acq_old_parent_in->i_st.st_nlink--;
            acq_new_parent_in->i_st.st_nlink++;
          }
          if(!dupl_reg_in->fake)
            dupl_reg_in->i_st.st_nlink--;

          fuse_reply_err(req,0);
          return 0;
        };
    };
  }
}



void FileSystem::replace_entry_same_parent(uint64_t  uid,cown_ptr<DirInode> old_parent_inode,acquired_cown<DirInode> &acq_old_parent_in,std::string newname, std::map<std::basic_string<char>, uint64_t>::iterator old_entry,fuse_req_t req ){
  //new entry = (old_name,ino)
  auto dupl_entry =acq_old_parent_in->dentries.find(newname);


  //new_name does not exist as an entry to new_parent_entries -- easy, just remove the old entry (oldname,ino) and add new entry (newname,ino)
  if(dupl_entry == acq_old_parent_in->dentries.end()){
    //delete <old_name,ino> for old_parent_dir
    //std::cout << "Erasing entry" << old_entry->first << " " << old_entry->second << " from dir with ino " << acq_old_parent_in->ino << std::endl;
    acq_old_parent_in->dentries.erase(old_entry->first);
    //add entry <new_name,ino>
    acq_old_parent_in->dentries.insert({newname,old_entry->second});

    //reply to kernel
    fuse_reply_err(req, 0);
  }
  // new_name already exists as an entry to new_parent_entries ==> decrement nlinks of this inode
  else{

    when(regular_inode_table,dir_inode_table) << [uid,old_entry,dupl_entry,old_parent_inode,req](auto reg_table,auto dir_table){
      cown_ptr<RegInode> old_reg_ino_cown = reg_table->find(old_entry->second) == reg_table->end() ? make_cown<RegInode>() : reg_table->find(old_entry->second)->second;
      cown_ptr<RegInode> dupl_reg_ino_cown = reg_table->find(dupl_entry->second) == reg_table->end() ? make_cown<RegInode>() : reg_table->find(dupl_entry->second)->second;

      cown_ptr<DirInode> old_dir_ino_cown = dir_table->find(old_entry->second) == dir_table->end() ? make_cown<DirInode>() : dir_table->find(old_entry->second)->second;
      cown_ptr<DirInode> dupl_dir_ino_cown = dir_table->find(dupl_entry->second) == dir_table->end() ? make_cown<DirInode>() : dir_table->find(dupl_entry->second)->second;


      when(old_parent_inode,
           old_reg_ino_cown,
           dupl_reg_ino_cown,
           old_dir_ino_cown,
           dupl_dir_ino_cown) << [=]
        (acquired_cown<DirInode> acq_old_parent_in,
         acquired_cown<RegInode> old_reg_in,
         acquired_cown<RegInode> dupl_reg_in,
         acquired_cown<DirInode> old_dir_in,
         acquired_cown<DirInode> dupl_dir_in){

          if (
            !dupl_reg_in->fake && acq_old_parent_in->i_st.st_mode & S_ISVTX && uid
            && uid != dupl_reg_in->i_st.st_uid && uid != acq_old_parent_in->i_st.st_uid) {
            reply_fail(-EPERM,req);
            return -EPERM;
          }

          if (
            !dupl_dir_in->fake && acq_old_parent_in->i_st.st_mode & S_ISVTX && uid
            && uid != dupl_reg_in->i_st.st_uid && uid != acq_old_parent_in->i_st.st_uid) {
            reply_fail(-EPERM,req);
            return -EPERM;
          }


          if( !old_dir_in->fake && (old_dir_in->i_st.st_mode & S_IFDIR)){
            if( !dupl_dir_in->fake && (dupl_dir_in->i_st.st_mode & S_IFDIR)  ){
              if(!dupl_dir_in->dentries.empty()){
                reply_fail(-ENOTEMPTY,req);
                return -ENOTEMPTY;
              }
            }
            else{
              reply_fail(-ENOTDIR,req);
              return -ENOTDIR;
            }
          }else {
            if (!dupl_dir_in->fake && dupl_dir_in->i_st.st_mode & S_IFDIR) return -EISDIR;
          }
          acq_old_parent_in->i_st.st_ctime = std::time(nullptr);

          acq_old_parent_in->dentries.erase(old_entry->first);
          acq_old_parent_in->dentries.erase(dupl_entry->first);
          acq_old_parent_in->dentries.insert({dupl_entry->first,old_entry->second});

          if(!old_dir_in->fake && !dupl_dir_in->fake){
            acq_old_parent_in->i_st.st_nlink--;
            //acq_old_parent_in->i_st.st_nlink++;
          }
          if(!dupl_reg_in->fake)
            dupl_reg_in->i_st.st_nlink--;

          fuse_reply_err(req,0);
          return 0;
        };


    };
  }
}



int FileSystem::rename(fuse_ino_t parent_ino, const std::string& oldname, fuse_ino_t newparent_ino, const std::string& newname, uid_t uid, gid_t gid, fuse_req_t req)
{
  if (oldname.length() > NAME_MAX || newname.length() > NAME_MAX)
    return -ENAMETOOLONG;

  if(parent_ino != newparent_ino){

    cown_ptr<DirInode> old_parent_inode = nullptr;
    if(parent_ino == 1)
      old_parent_inode = root;
    else{
      auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
      it->acquire_strong_from_weak();
      old_parent_inode  = cown_ptr<DirInode>(it);
    }


    cown_ptr<DirInode> new_parent_inode = nullptr;
    if(newparent_ino == 1)
      new_parent_inode = root;
    else{
      auto it = reinterpret_cast<ActualCown<DirInode> *>(newparent_ino);
      it->acquire_strong_from_weak();
      new_parent_inode  = cown_ptr<DirInode>(it);
    }


    when(old_parent_inode, new_parent_inode) << [=](acquired_cown<DirInode> acq_old_parent_in,acquired_cown<DirInode>  acq_new_parent_in){

      //old entry = (old_name,ino)
      auto old_entry = acq_old_parent_in->dentries.find(oldname);
      // check if old entry does not exist
      if (old_entry == acq_old_parent_in->dentries.end()) {
        reply_fail(-ENOENT,req);
        return -ENOENT;
      }

      int ret = access(acq_old_parent_in->i_st, W_OK, uid, gid);
      if (ret) {
        fuse_reply_err(req,-ret);
        return ret;
      }

      ret = access(acq_new_parent_in->i_st, W_OK, uid, gid);
      if (ret) {
        fuse_reply_err(req,-ret);
        return ret;
      }

      //Inode * old_in = reinterpret_cast<Inode *>(old_entry->second);

      if(old_entry->second==1 || old_entry->second%2==0){

        cown_ptr<DirInode> dirInode = nullptr;
        if(old_entry->second==1)
          dirInode = root;
        else{
          auto it = reinterpret_cast<ActualCown<DirInode> *>(old_entry->second);
          it->acquire_strong_from_weak();
          dirInode = cown_ptr<DirInode>(it);
        }

        when(dirInode,old_parent_inode,new_parent_inode) << [old_parent_inode,new_parent_inode,newname,old_entry,req,this,uid,gid](auto old_in,auto acq_old_parent_in,auto acq_new_parent_in){
          if (old_in->i_st.st_mode & S_IFDIR) {
            auto ret = access(old_in->i_st, W_OK, uid, gid);
            if (ret) return ret;
          }

          if (acq_old_parent_in->i_st.st_mode & S_ISVTX) {
            if (uid && uid != old_in->i_st.st_uid && uid != acq_old_parent_in->i_st.st_uid)
              return -EPERM;
          }

          replace_entry(old_parent_inode,new_parent_inode,uid,acq_old_parent_in,acq_new_parent_in,newname,old_entry,req);
          return 0;
        };
      }else{


        auto it = reinterpret_cast<ActualCown<RegInode> *>(old_entry->second-1);
        it->acquire_strong_from_weak();
        auto regInode = cown_ptr<RegInode>(it);


        when(regInode,old_parent_inode,new_parent_inode) << [old_parent_inode,new_parent_inode,newname,old_entry,req,this,uid,gid](acquired_cown<RegInode> old_in,auto acq_old_parent_in,auto acq_new_parent_in){
          if (old_in->i_st.st_mode & S_IFDIR) {
            auto ret = access(old_in->i_st, W_OK, uid, gid);
            if (ret) return ret;
          }

          if (acq_old_parent_in->i_st.st_mode & S_ISVTX) {
            if (uid && uid != old_in->i_st.st_uid && uid != acq_old_parent_in->i_st.st_uid)
              return -EPERM;
          }

          replace_entry(old_parent_inode,new_parent_inode,uid,acq_old_parent_in,acq_new_parent_in,newname,old_entry,req);
          return 0;
        };
      }
      return 0;
    };
  }
  else{


    cown_ptr<DirInode> old_parent_inode= nullptr;
    if(parent_ino==1)
      old_parent_inode=root;
    else{
      auto it = reinterpret_cast<ActualCown<DirInode> *>(parent_ino);
      it->acquire_strong_from_weak();
      old_parent_inode  = cown_ptr<DirInode>(it);
    }

    when(old_parent_inode) << [=](acquired_cown<DirInode> acq_old_parent_in){


      //old entry = (old_name,ino)
      auto old_entry = acq_old_parent_in->dentries.find(oldname);
      // check if old entry does not exist
      if (old_entry == acq_old_parent_in->dentries.end()) {
        reply_fail(-ENOENT,req);
        return -ENOENT;
      }

      int ret = access(acq_old_parent_in->i_st, W_OK, uid, gid);
      if (ret) {
        fuse_reply_err(req,-ret);
        return ret;
      }

      ret = access(acq_old_parent_in->i_st, W_OK, uid, gid);
      if (ret) {
        fuse_reply_err(req,-ret);
        return ret;
      }




      if(old_entry->second == 1 || old_entry->second%2==0){

        cown_ptr<DirInode> old_entry_inode = nullptr;
        if(old_entry->second==1)
          old_entry_inode = root;
        else{
          auto it = reinterpret_cast<ActualCown<DirInode> *>(old_entry->second);
          it->acquire_strong_from_weak();
          old_entry_inode = cown_ptr<DirInode>(it);
        }

        //------

        when(old_entry_inode,old_parent_inode) <<[this,req,uid,gid,old_parent_inode,newname,old_entry](auto old_in,auto acq_old_parent_in){
          if (old_in->i_st.st_mode & S_IFDIR) {
            auto ret = access(old_in->i_st, W_OK, uid, gid);
            if (ret) {
              reply_fail(ret,req);
              return ret;
            }
          }

          if (old_in->i_st.st_mode & S_ISVTX) {
            if (uid && uid != old_in->i_st.st_uid && uid != old_in->i_st.st_uid)
              return -EPERM;
          }

          replace_entry_same_parent(uid,old_parent_inode,acq_old_parent_in,newname,old_entry,req);
          return 0;
        };
      }else{


        auto it = reinterpret_cast<ActualCown<RegInode> *>(old_entry->second-1);
        it->acquire_strong_from_weak();
        auto regInode = cown_ptr<RegInode>(it);

        when(regInode,old_parent_inode) << [this,uid,gid,old_parent_inode,newname,old_entry,req](auto old_in,auto acq_old_parent_in){
          if (old_in->i_st.st_mode & S_IFDIR) {
            auto ret = access(old_in->i_st, W_OK, uid, gid);
            if (ret) return ret;
          }

          if (acq_old_parent_in->i_st.st_mode & S_ISVTX) {
            if (uid && uid != old_in->i_st.st_uid && uid != acq_old_parent_in->i_st.st_uid)
              return -EPERM;
          }
          replace_entry_same_parent(uid,old_parent_inode,acq_old_parent_in,newname,old_entry,req);
          return 0;
        };





      }
      return 0;
    };

  };
  return 0;
}

void FileSystem::forget(fuse_ino_t ino, unsigned long nlookup,fuse_req_t req)
{

  // if inode number indicates a directory inode
  if( ino==1 || ino%2==0){

    cown_ptr<DirInode> dirInode = nullptr;
    if(ino==1)
      dirInode = root;
    else
    {
      auto it = reinterpret_cast<ActualCown<DirInode> *>(ino);
      it->acquire_strong_from_weak();
      dirInode = cown_ptr<DirInode>(it);
    }

    when(dirInode,dir_inode_table) << [ino,nlookup](acquired_cown<DirInode> dir_ino,auto dir_table){
      assert(dir_ino->krefs>0);
      dir_ino->krefs -= nlookup;
      assert(dir_ino->krefs>=0);
      if(dir_ino->krefs == 0  && dir_ino->i_st.st_nlink == 0){
        //printf("Erasing dir-ino:%lu with nlookup: %lud and nlinks: %lu",dir_ino->i_st.st_ino,dir_ino->krefs,dir_ino->i_st.st_nlink);
        dir_table->erase(ino);
      }
    };
  }else{
    auto it = reinterpret_cast<ActualCown<RegInode> *>(ino-1);
    it->acquire_strong_from_weak();
    cown_ptr<RegInode> regInode = cown_ptr<RegInode>(it);
    when(regInode,regular_inode_table) << [=](acquired_cown<RegInode> reg_ino,auto reg_table){
      assert(reg_ino->krefs>0);
      reg_ino->krefs -= nlookup;
      assert(reg_ino->krefs>=0);
      if(reg_ino->krefs == 0 && reg_ino->i_st.st_nlink == 0){
        //printf("Erasing reg-ino:%lu with nlookup: %lud and nlinks: %lu",reg_ino->i_st.st_ino,reg_ino->krefs,reg_ino->i_st.st_nlink);
        reg_table->erase(ino);
        for(auto &pair: reg_ino->data_blocks){
          cown_ptr<Block> block = pair.second;
          when(block) << [=](acquired_cown<Block> blk){
            free_space(blk);
          };
        }
      }
    };
  }
}


void my_test(int argc,char *argv[],SystematicTestHarness *harness){
  when()<<[=](){

    Scheduler::add_external_event_source();
    harness->external_thread([=]() {

      setbuf(stdout,0);
      int returnValue = system("fusermount -u /home/sevag/ssfs");

      if (returnValue == -1) {
        // Error executing the command
        perror("system");
        exit(EXIT_FAILURE);
      }

      struct filesystem_opts opts;
      // option defaults

      opts.size = 512 << 20;
      opts.debug = false;
      struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
      if (fuse_opt_parse(&args, &opts, fs_fuse_opts, fs_opt_proc) == -1) {
        exit(1);
      }

      assert(opts.size > 0);



      const char* mountpoint = "/home/sevag/ssfs";



      FileSystem fs(opts.size);

      auto se = fuse_session_new(&args,&fs.ops(), sizeof(fs.ops()),&fs);
      fuse_set_signal_handlers(se);
      fuse_session_mount(se,mountpoint);
      fuse_daemonize(true);

      fuse_session_loop(se);
      fuse_session_unmount(se);
      fuse_remove_signal_handlers(se);
      fuse_session_destroy(se);

    });
  };


}





int main(int argc, char *argv[])
{
  SystematicTestHarness harness(argc,argv);
  Scheduler& sched = Scheduler::get();
  sched.init(16);
  my_test(argc,argv,&harness);
  sched.run();
  while (!harness.external_threads.empty())
  {
    auto& thread = harness.external_threads.front();
    thread.join();
    harness.external_threads.pop_front();
  }

  Logging::cout() << "External threads joined" << std::endl;

  LocalEpochPool::sort();

  if (harness.detect_leaks)
    snmalloc::debug_check_empty<snmalloc::Alloc::Config>();



  //SystematicTestHarness harness(argc,argv);
  //harness.run(my_test,argc,argv,&harness);
}