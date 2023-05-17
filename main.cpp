
/** @file
 *
 * minimal example filesystem using low-level API
 *
 * Compile with:
 *     gcc -Wall hello_ll.c `pkg-config fuse3 --cflags --libs` -o hello_ll
 *     ./hello_ll -f /tmp/ssfs
 */

#define FUSE_USE_VERSION 34

#include "main.h"

#include "dirent.h"
#include "fuse3/fuse.h"
#include "fuse3/fuse_opt.h"
#include "pthread.h"

#include <assert.h>
#include <cpp/when.h>
#include <debug/harness.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse3/fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BLOCK_SIZE 32ul

// -----------------------------------------------------------
// ------------------------- Definitions ---------------------
using namespace verona::cpp;
struct ExternalSource;

struct FileSystem;

struct Block {
  Block(size_t size)
  : size(size)
    , buf(new char[size]) {}

  size_t size;
  std::unique_ptr<char[]> buf;

  void write(char* data, uint64_t bytes_to_write, uint64_t block_offset,uint64_t bytes_written);
};

void Block::write(char* data, uint64_t bytes_to_write, uint64_t block_offset,uint64_t bytes_written)
{
  memcpy(buf.get()+block_offset,data+bytes_written,bytes_to_write);
}

class Inode {
public:
  Inode(
    fuse_ino_t ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    mode_t mode,
    FileSystem* fs)
  : ino(ino)
    , fs_(fs) {
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
  FileSystem* fs_;
};

class RegInode : public Inode {
public:
  RegInode(
    fuse_ino_t ino,
    time_t time,
    uid_t uid,
    gid_t gid,
    blksize_t blksize,
    mode_t mode,
    FileSystem* fs)
  : Inode(ino, time, uid, gid, blksize, mode, fs) {
    i_st.st_nlink = 1;
    i_st.st_mode = S_IFREG | mode;
  }

  ~RegInode();

  void write(char *buf,size_t size,off_t offset,fuse_req_t req);

  std::map<off_t, cown_ptr<Block>> data_blocks;
};


class Counter {
public:
  u_int64_t count = 0;
};

// Someone acquires a regular inode and writes through it
// when (regInode) << []() { ... regInode.write() ... }
void RegInode::write(char* buf, size_t size, off_t offset,fuse_req_t req)
{
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
      data_blocks.emplace(blockId, make_cown<Block>(BLOCK_SIZE));
      // FIXME AVAILABLE SPACE COUNTER SHOULD BE A COWN
      // FIXME: BEFORE ALLOCATING BLOCK CHECK THAT THERE IS AVAILABLE SPACE AND RETURN ERROR
      // int ret = ...
      // fuse_reply_err(req, -ret);
    }

    size_t bytes_to_write = std::min(BLOCK_SIZE - block_offset, remaining_size);

    // TODO: Last time we said that we could have multiple blocks within a when,
    //  but the only way to do this would be to collect all cowns together somehow, but how?
    when(data_blocks.at(blockId),counter) << [=] (acquired_cown<Block> blk,auto ctr){
      blk->write(buf,bytes_to_write,block_offset,bytes_written);
      //memcpy(data_blocks.at(blockId).buf.get()+block_offset,buf+bytes_written,bytes_to_write);
      ctr->count += bytes_to_write;
    };
    bytes_written += bytes_to_write;
    remaining_size -= bytes_to_write;
  }

  // TODO: PROBLEM IS THAT WE HAVE TO REPLY TO THE KERNEL WHEN THE FOR LOOP FINISHES
  //  USING THIS HACK I MANAGED TO SERIALIZE THE EXECUTION OF THE LOOP, IS THAT FINE?
  when(counter) << [=] (acquired_cown<Counter> ctr){
    if (ctr->count == size) {
      // All writes have completed, send the reply to the kernel
      fuse_reply_write(req, ctr->count);
    }
  };

  //if (ret >= 0)
  //  fuse_reply_write(req, ret);
  //else
  //  fuse_reply_err(req, -ret);

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
    mode_t mode,
    FileSystem* fs)
  : Inode(ino, time, uid, gid, blksize, mode, fs) {
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

struct FileHandle {
  cown_ptr<RegInode> in;
  int flags;

  FileHandle(cown_ptr<RegInode> in, int flags)
  : in(in)
    , flags(flags) {}
};
// TODO: READ,WRITE,RENAME,UNLINK,RELEASE
class FileSystem : public filesystem_base{
public:
  FileSystem(size_t size);
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
  int create(fuse_ino_t parent_ino,const std::string& name,mode_t mode,int flags,uid_t uid,gid_t gid, fuse_req_t req,struct fuse_file_info* fi);
  int getattr(fuse_ino_t ino,  uid_t uid, gid_t gid, fuse_req_t req);
  ssize_t readdir(fuse_req_t req, fuse_ino_t ino,  size_t bufsize, off_t off);
  int mkdir(fuse_ino_t parent_ino,const std::string& name,mode_t mode, uid_t uid,gid_t gid, fuse_req_t req);
  //int mknod(fuse_ino_t parent_ino,const std::string& name,mode_t mode,dev_t rdev,struct stat* st,uid_t uid,gid_t gid);
  int access(fuse_ino_t ino, int mask, uid_t uid, gid_t gid,fuse_req_t req);
  int setattr(fuse_ino_t ino,FileHandle* fh,struct stat* attr,int to_set,uid_t uid,gid_t gid,fuse_req_t req);
  int lookup(fuse_ino_t parent_ino, const std::string& name,fuse_req_t req);
  void init(void *userdata,struct fuse_conn_info *conn);
  int open(fuse_ino_t ino, int flags, FileHandle** fhp, uid_t uid, gid_t gid,  struct fuse_file_info* fi,fuse_req_t req);
  ssize_t write(FileHandle* fh, const char * buf, size_t size, off_t off, struct fuse_file_info *fi,fuse_req_t req);
  // helper functions
private:
  void add_inode(cown_ptr<DirInode> inode);
  void add_inode(cown_ptr<RegInode> inode);
  ssize_t write(const std::shared_ptr<RegInode>& in,off_t offset,size_t size,const char* buf);

  void get_inode(cown_ptr<RegInode> inode_cown, acquired_cown<RegInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>> &regular_inode_table);
  void get_inode(cown_ptr<DirInode> inode_cown, acquired_cown<DirInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>> &dir_inode_table);
  // private fields;
  cown_ptr<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>> regular_inode_table = make_cown<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>>();
  cown_ptr<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>> dir_inode_table = make_cown<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>>();

private:
  int access(struct stat st, int mask, uid_t uid, gid_t gid);
  void init_stat(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode,struct stat *i_st,bool is_regular);

private:
  std::atomic<fuse_ino_t> next_ino_;
  size_t avail_bytes_;
  struct statvfs stat;
  cown_ptr<DirInode> root;
};

// Implementation

//TODO: REMAINING OPERATIONS ARE: RELEASE,READ,WRITE--ALMOST DONE,RENAME,UNLINK,RMDIR,FORGET

void FileSystem::init_stat(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode,struct stat *i_st,bool is_regular)
{
  memset(i_st, 0, sizeof(i_st));
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


void FileSystem::add_inode(cown_ptr<DirInode> ino)
{
  when(ino,dir_inode_table) << [=](acquired_cown<DirInode> inode,auto table){
    assert(inode->krefs == 0);
    inode->krefs++;
    [[maybe_unused]] auto res = table->emplace(inode->ino, ino);
  };
}

void FileSystem::add_inode(cown_ptr<RegInode> ino)
{
  when(ino,regular_inode_table) << [=](acquired_cown<RegInode> inode,auto table){
    assert(inode->krefs == 0);
    inode->krefs++;
    [[maybe_unused]] auto res = table->emplace(inode->ino, ino);
  };
}

void FileSystem::get_inode(cown_ptr<RegInode> inode_cown, acquired_cown<RegInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<RegInode>>> &regular_inode_table)
{
  acq_inode->krefs++;
  auto res = regular_inode_table->emplace(acq_inode->ino, inode_cown);
  if (!res.second) {
    assert(acq_inode->krefs > 0);
  } else {
    assert(acq_inode->krefs == 0);
  }
}

void FileSystem::get_inode(cown_ptr<DirInode> inode_cown, acquired_cown<DirInode> &acq_inode,  acquired_cown<std::unordered_map<fuse_ino_t, cown_ptr<DirInode>>> &dir_inode_table)
{
  acq_inode->krefs++;
  auto res = dir_inode_table->emplace(acq_inode->ino, inode_cown);
  if (!res.second) {
    assert(acq_inode->krefs > 0);
  } else {
    assert(acq_inode->krefs == 0);
  }
}


FileSystem::FileSystem(size_t size): next_ino_(FUSE_ROOT_ID) {
  auto now = std::time(nullptr);

  auto node_id = next_ino_++;
  printf("node id is :%lu\n",node_id);

  root = make_cown<DirInode>(node_id, now, getuid(), getgid(), 4096, 0755, this);
  avail_bytes_ = size;

  memset(&stat, 0, sizeof(stat));
  stat.f_fsid = 983983;
  stat.f_namemax = PATH_MAX;
  stat.f_bsize = 4096;
  stat.f_frsize = 4096;
  stat.f_blocks = size / 4096;
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
  add_inode(root);
  printf("want %d\n",conn->want);
  conn->want |= FUSE_CAP_PARALLEL_DIROPS;
  printf("want %d\n",conn->want);
}

int FileSystem::lookup(fuse_ino_t parent_ino, const std::string& name, fuse_req_t req)
{

  when(dir_inode_table) << [=](auto dir_table){


    cown_ptr<DirInode> parent_in = dir_table->at(parent_ino);
    when(parent_in) << [=](auto  parent_in){
      DirInode::dir_t::const_iterator it = parent_in->dentries.find(name);
      if (it == parent_in->dentries.end()) {
        //log_->debug("lookup parent {} name {} not found", parent_ino, name);
        reply_fail(-ENOENT,req);
        return -ENOENT;
      }

      auto in = it->second;
      when(regular_inode_table,dir_inode_table) << [req,in, this](auto reg_table,auto dir_table){
        //TODO Maybe check that inode indeed exists otherwise return ENOENT

        if( reg_table->find(in) != reg_table->end()){
          auto inode = reg_table->at(in);
          when(inode,regular_inode_table) << [req,this,inode](acquired_cown<RegInode> acq_inode,auto  reg_table){
            get_inode(inode,acq_inode,reg_table);
            struct fuse_entry_param fe;
            std::memset(&fe, 0, sizeof(fe));
            fe.attr = acq_inode->i_st;
            reply_lookup_success(fe,req);
            return 0;
          };
        }


        if(dir_table->find(in) != dir_table->end()){
          auto inode = dir_table->at(in);
          when(inode,dir_inode_table) << [req,this,inode](acquired_cown<DirInode> acq_inode,auto  dir_table){
            get_inode(inode,acq_inode,dir_table);
            struct fuse_entry_param fe;
            std::memset(&fe, 0, sizeof(fe));
            fe.attr = acq_inode->i_st;
            reply_lookup_success(fe,req);
            return 0;
          };
        }

      };

    };
  };
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

int FileSystem::create(fuse_ino_t parent_ino, const std::string& name, mode_t mode, int flags, uid_t uid, gid_t gid, fuse_req_t req, struct fuse_file_info* fi)
{
  if(name.length() > NAME_MAX){
    std::cout << "name: " << name << "is too long" << std::endl;
    reply_fail(-ENAMETOOLONG, req);
    return -ENAMETOOLONG;
  }

  auto now = std::time(nullptr);
  auto node_id = next_ino_++;
  printf("node id is :%lu\n",node_id);

  // First acquires directory_inode_table which is a cown
  // then acquire DirInode which is also a cown and adds an entry to it
  // also add inode to reg inode table
  when(dir_inode_table) << [=](auto dir_table){
    cown_ptr<DirInode> parent_in = dir_table->at(parent_ino);
    when(parent_in) << [=](acquired_cown<DirInode> parent_in){

      auto in = make_cown<RegInode>(node_id, now, uid, gid, 4096, S_IFREG | mode, this);
      struct stat i_st;
      init_stat(node_id,now,uid,gid,4096,S_IFREG | mode,&i_st, true);

      auto fh = std::make_unique<FileHandle>(in, flags);
      DirInode::dir_t& children = parent_in->dentries;

      if (children.find(name) != children.end()) {
        std::cout << "create name" << name << " already exists" << std::endl;
        reply_fail(-EEXIST, req);
        return -EEXIST;
      }

      int ret = access(parent_in->i_st, W_OK, uid, gid);
      if (ret) {
        //log_->debug("create name {} access denied ret {}", name, ret);
        reply_fail(ret, req);
        return ret;
      }

      children[name]= node_id;
      add_inode(in);

      parent_in->i_st.st_ctime = now;
      parent_in->i_st.st_mtime = now;


      FileHandle* fhp = fh.release();
      reply_create_success(fi,fhp,req,i_st);
    };
  };
}

ssize_t FileSystem::readdir(fuse_req_t req, fuse_ino_t parent_ino, size_t bufsize, off_t toff)
{
  // Again same pattern, first acquire dir_inode_table
  // then dir inode and then iterate through its entries
  when(dir_inode_table) << [=](auto dir_table){
    size_t off = toff;
    auto temp = std::unique_ptr<char[]>(new char[bufsize]);
    char *buf = temp.get();


    size_t pos = 0;

    /*
     * FIXME: the ".." directory correctly shows up at the parent directory
     * inode, but "." shows a inode number as "?" with ls -lia.
     */
    if (off == 0) {
      size_t remaining = bufsize - pos;
      struct stat st;
      memset(&st, 0, sizeof(st));
      st.st_ino = 1;
      size_t used = fuse_add_direntry(req, buf + pos, remaining, ".", &st, 1);
      if (used > remaining){
        reply_readdir(req,buf,pos);
        return pos;
      }
      printf("Str %s",buf+pos);
      pos += used;
      off = 1;
    }



    if (off == 1) {
      size_t remaining = bufsize - pos;
      struct stat st;
      memset(&st, 0, sizeof(st));
      st.st_ino = 1;
      size_t used = fuse_add_direntry(
        req, buf + pos, remaining, "..", &st, 2);
      if (used > remaining) {
        reply_readdir(req,buf,pos);
        return pos;
      }
      printf("Str %s",buf+pos);
      pos += used;
      off = 2;
    }

    assert(off >= 2);
    cown_ptr<DirInode> parent_in = dir_table->at(parent_ino);
    when(parent_in) << [=](auto acq_parent_in){
      auto cp_pos = pos;
      auto cp_off = off;
      DirInode::dir_t& children = acq_parent_in->dentries;

      size_t count = 0;
      size_t target = cp_off - 2;

      for (DirInode::dir_t::const_iterator it = children.begin();it != children.end();it++) {
        if (count >= target) {
          auto in = it->second;
          struct stat st;
          memset(&st, 0, sizeof(st));
          st.st_ino = in;
          size_t remaining = bufsize - cp_pos;
          size_t used = fuse_add_direntry(
            req, buf + cp_pos, remaining, it->first.c_str(), &st, cp_off + 1);
          if (used > remaining){
            reply_readdir(req,buf,cp_pos);
            return cp_pos;
          }
          cp_pos += used;
          cp_off++;
        }
        count++;
      }
      reply_readdir(req,buf,cp_pos);
    };
  };
}

int FileSystem::getattr(fuse_ino_t ino, uid_t uid, gid_t gid, fuse_req_t req)
{
  when(dir_inode_table,regular_inode_table) << [=](auto dir_table,auto reg_table){
    auto it = dir_table->find(ino);
    if(it != dir_table->end()){
        cown_ptr<DirInode> dirInode = it->second;
        when(dirInode) << [req](acquired_cown<DirInode> dir_ino){
          struct stat st = dir_ino->i_st;
          fuse_reply_attr(req, &st, 0);
        };
    }
    else{
        auto it = reg_table->find(ino);
        if(it != reg_table->end()){
          cown_ptr<RegInode> regInode = it->second;
          when(regInode) << [req](acquired_cown<RegInode> reg_ino){
            struct stat st = reg_ino->i_st;
            fuse_reply_attr(req, &st, 0);
          };
        }
    }

  };
}

int FileSystem::setattr(fuse_ino_t ino, FileHandle* fh, struct stat* attr, int to_set, uid_t uid, gid_t gid, fuse_req_t req)
{
  //Case where it's a regular inode
  when(regular_inode_table) << [=](auto reg_table){
    auto it = reg_table->find(ino);
    if(it == reg_table->end())
      return -1;
    cown_ptr<RegInode> reg_inode =  reg_table->at(ino);
    when(reg_inode,regular_inode_table) << [=]( acquired_cown<RegInode> reg_inode,auto){

      mode_t clear_mode = 0;
      struct stat i_st = reg_inode->i_st;
      auto now = std::time(nullptr);
      if (to_set & FUSE_SET_ATTR_MODE) {
        if (uid && i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && i_st.st_gid != gid) clear_mode |= S_ISGID;

        i_st.st_mode = attr->st_mode;
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
          && (i_st.st_uid != attr->st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && (to_set & FUSE_SET_ATTR_GID) && (uid != i_st.st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (to_set & FUSE_SET_ATTR_UID) i_st.st_uid = attr->st_uid;

        if (to_set & FUSE_SET_ATTR_GID) i_st.st_gid = attr->st_gid;
      }

      if (to_set & (FUSE_SET_ATTR_MTIME | FUSE_SET_ATTR_ATIME)) {
        if (uid && i_st.st_uid != uid)
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

#ifdef FUSE_SET_ATTR_MTIME_NOW
        if (to_set & FUSE_SET_ATTR_MTIME_NOW)
          i_st.st_mtime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_MTIME)
          i_st.st_mtime = attr->st_mtime;

#ifdef FUSE_SET_ATTR_ATIME_NOW
        if (to_set & FUSE_SET_ATTR_ATIME_NOW)
          i_st.st_atime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_ATIME)
          i_st.st_atime = attr->st_atime;
      }

#ifdef FUSE_SET_ATTR_CTIME
      if (to_set & FUSE_SET_ATTR_CTIME) {
        if (uid && i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }
        i_st.st_ctime = attr->st_ctime;
      }
#endif

      if (to_set & FUSE_SET_ATTR_SIZE) {
        if (uid) {     // not root
          if (!fh) { // not open file descriptor
            int ret = access(i_st, W_OK, uid, gid);
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
        if (attr->st_size > 2199023255552) {
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

        i_st.st_mtime = now;
      }
      i_st.st_ctime = now;
      if (to_set & FUSE_SET_ATTR_MODE) i_st.st_mode &= ~clear_mode;

      *attr = i_st;
      fuse_reply_attr(req, attr, 0);
      return 0;
    };
  };


  //Case where it's a dir inode
  when(dir_inode_table) << [=](auto dir_table){
    auto it = dir_table->find(ino);
    if(it == dir_table->end())
      return -1;
    cown_ptr<DirInode> dir_inode =  dir_table->at(ino);
    when(dir_inode,dir_inode_table) << [=]( acquired_cown<DirInode> reg_inode,auto ){

      mode_t clear_mode = 0;
      struct stat i_st = reg_inode->i_st;
      auto now = std::time(nullptr);
      if (to_set & FUSE_SET_ATTR_MODE) {
        if (uid && i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && i_st.st_gid != gid) clear_mode |= S_ISGID;

        i_st.st_mode = attr->st_mode;
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
          && (i_st.st_uid != attr->st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (uid && (to_set & FUSE_SET_ATTR_GID) && (uid != i_st.st_uid))
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

        if (to_set & FUSE_SET_ATTR_UID) i_st.st_uid = attr->st_uid;

        if (to_set & FUSE_SET_ATTR_GID) i_st.st_gid = attr->st_gid;
      }

      if (to_set & (FUSE_SET_ATTR_MTIME | FUSE_SET_ATTR_ATIME)) {
        if (uid && i_st.st_uid != uid)
        {
          reply_fail(-EPERM,req);
          return -EPERM;
        }

#ifdef FUSE_SET_ATTR_MTIME_NOW
        if (to_set & FUSE_SET_ATTR_MTIME_NOW)
          i_st.st_mtime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_MTIME)
          i_st.st_mtime = attr->st_mtime;

#ifdef FUSE_SET_ATTR_ATIME_NOW
        if (to_set & FUSE_SET_ATTR_ATIME_NOW)
          i_st.st_atime = std::time(nullptr);
        else
#endif
          if (to_set & FUSE_SET_ATTR_ATIME)
          i_st.st_atime = attr->st_atime;
      }

#ifdef FUSE_SET_ATTR_CTIME
      if (to_set & FUSE_SET_ATTR_CTIME) {
        if (uid && i_st.st_uid != uid) {
          reply_fail(-EPERM,req);
          return -EPERM;
        }
        i_st.st_ctime = attr->st_ctime;
      }
#endif

      if (to_set & FUSE_SET_ATTR_SIZE) {
        if (uid) {     // not root
          if (!fh) { // not open file descriptor
            int ret = access(i_st, W_OK, uid, gid);
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
        if (attr->st_size > 2199023255552) {
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

        i_st.st_mtime = now;
      }
      i_st.st_ctime = now;
      if (to_set & FUSE_SET_ATTR_MODE) i_st.st_mode &= ~clear_mode;

      *attr = i_st;
      fuse_reply_attr(req, attr, 0);
      return 0;
    };
  };

}

int FileSystem::access(fuse_ino_t ino, int mask, uid_t uid, gid_t gid, fuse_req_t req)
{
  when(regular_inode_table) << [=](auto reg_table){
     auto it = reg_table->find(ino);
     if(it != reg_table->end()){
       cown_ptr<RegInode> reg_inode =  reg_table->at(ino);
       when(regular_inode_table,reg_inode) << [=](auto reg_table,auto reg_inode){
         int ret = access(reg_inode->i_st, mask, uid, gid);
         fuse_reply_err(req, -ret);
       };
     }
  };

  when(dir_inode_table) << [=](auto dir_table){
    auto it = dir_table->find(ino);
    if(it != dir_table->end()){
      cown_ptr<DirInode> dir_inode =  dir_table->at(ino);
      when(dir_inode_table,dir_inode) << [=](auto dir_table,auto dir_inode){
        int ret = access(dir_inode->i_st, mask, uid, gid);
        fuse_reply_err(req, -ret);
      };
    }
  };
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
  when(dir_inode_table) << [=](auto dir_table) {
    cown_ptr<DirInode> parent_in = dir_table->at(parent_ino);
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

      auto node_id = next_ino_++;
      struct stat i_st;
      cown_ptr<DirInode> in = make_cown<DirInode>(node_id, now, uid, gid, 4096, mode, this);
      init_stat(node_id, now, uid, gid, 4096, mode,&i_st, false);
      children[name] = node_id;
      add_inode(in);

      parent_in->i_st.st_ctime = now;
      parent_in->i_st.st_mtime = now;
      parent_in->i_st.st_nlink++;

      reply_mkdir(i_st,req);
      return 0;
    };
  };
}

int FileSystem::open(fuse_ino_t ino, int flags, FileHandle** fhp, uid_t uid, gid_t gid, struct fuse_file_info* fi,fuse_req_t req)
{
  int mode = 0;
  if ((flags & O_ACCMODE) == O_RDONLY)
    mode = R_OK;
  else if ((flags & O_ACCMODE) == O_WRONLY)
    mode = W_OK;
  else if ((flags & O_ACCMODE) == O_RDWR)
    mode = R_OK | W_OK;

  if (!(mode & W_OK) && (flags & O_TRUNC)) {
    const int ret = -EACCES;
    fuse_reply_err(req,ret);
    return ret;
  }

  when(regular_inode_table) << [=](auto reg_table){
    //if( reg_table->find(in) != reg_table->end()){
    //  auto inode = reg_table->at(in);
    auto it = reg_table->find(ino);
    assert(it != reg_table->end());
    cown_ptr<RegInode> reg_inode = it->second;
    when(reg_inode) << [=](acquired_cown<RegInode> acquiredCown){
      auto fh = std::make_unique<FileHandle>(reg_inode, flags);
      int ret = access(acquiredCown->i_st, mode, uid, gid);

      if (ret) {
        reply_fail(ret,req);
        return ret;
      }

      // TODO Support Open with Truncate flag

      //if (flags & O_TRUNC) {
      //  ret = truncate(in, 0, uid, gid);
      //  if (ret) {
      //    reply_fail(ret,req);
      //    return ret;
      //  }
      //  auto now = std::time(nullptr);
      //  acquiredCown->i_st.st_mtime = now;
      //  acquiredCown->i_st.st_ctime = now;
      //}

      *fhp = fh.release();
      fi->fh = reinterpret_cast<uint64_t>(*fhp);
      fuse_reply_open(req, fi);
      return 0;
    };
  };
}

ssize_t FileSystem::write(FileHandle* fh, const char* buf, size_t size, off_t off, struct fuse_file_info* fi, fuse_req_t req)
{
  cown_ptr<RegInode> reg_inode = fh->in;


}








int main(int argc, char *argv[])
{

  cown_ptr<RegInode> reg1 = make_cown<RegInode>(5, 5, 5, 5, 4096, S_IFREG | 5, nullptr);
  cown_ptr<RegInode> reg2 = make_cown<RegInode>(5, 5, 5, 5, 4096, S_IFREG | 5, nullptr);


  schedule_lambda(2,arr,  []() { std::cout << "Hello world!\n"; }  );

  //char buf[4096] = "helloworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworldworld";
  //in->write(buf, strlen(buf),11500, reinterpret_cast<fuse_req_t>(5));

  //exit(1);
  SystematicTestHarness harness(argc, argv);
  struct filesystem_opts opts;
  // option defaults
  opts.size = 512 << 20;
  opts.debug = false;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  if (fuse_opt_parse(&args, &opts, fs_fuse_opts, fs_opt_proc) == -1) {
    exit(1);
  }

  assert(opts.size > 0);

  struct fuse_chan* ch;
  int err = -1;
  char* mountpoint = "/tmp/ssfs";



  FileSystem fs(opts.size);
  const fuse_lowlevel_ops& ops =  fs.ops();
  auto se = fuse_session_new(&args,&fs.ops(), sizeof(fs.ops()),&fs);
  fuse_set_signal_handlers(se);
  fuse_session_mount(se,mountpoint);
  fuse_daemonize(true);
  harness.run(my_session_loop,&harness,se);
  //fuse_session_loop(se);
  fuse_session_unmount(se);
  fuse_remove_signal_handlers(se);
  fuse_session_destroy(se);
}
