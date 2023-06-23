#pragma once
#define FUSE_USE_VERSION 35

#include "cpp/when.h"
#include "debug/harness.h"

#include <atomic>
#include <cassert>
#include <csignal>
#include <cstring>
#include <fuse3/fuse_lowlevel.h>
#include <linux/limits.h>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

using namespace verona::cpp;
struct FileHandle;

struct fuse_req {
  struct fuse_session *se;
  uint64_t unique;
  int ctr;
  pthread_mutex_t lock;
  struct fuse_ctx ctx;
  struct fuse_chan *ch;
  int interrupted;
  unsigned int ioctl_64bit : 1;
  union {
    struct {
      uint64_t unique;
    } i;
    struct {
      fuse_interrupt_func_t func;
      void *data;
    } ni;
  } u;
  struct fuse_req *next;
  struct fuse_req *prev;
  int *buf_mem;
};

struct filesystem_opts {
  size_t size;
  bool debug;
};

enum {
  KEY_HELP,
};

#define FS_OPT(t, p, v)                                                        \
    { t, offsetof(struct filesystem_opts, p), v }

static struct fuse_opt fs_fuse_opts[] = {
  FS_OPT("size=%llu", size, 0),
  FS_OPT("-debug", debug, 1),
  FUSE_OPT_KEY("-h", KEY_HELP),
  FUSE_OPT_KEY("--help", KEY_HELP),
  FUSE_OPT_END};


static void usage(const char* progname) {
  printf("file system options:\n"
    "    -o size=N          max file system size (bytes)\n"
    "    -debug             turn on verbose logging\n");
}



static int
fs_opt_proc(void* data, const char* arg, int key, struct fuse_args* outargs) {
  switch (key) {
    case FUSE_OPT_KEY_OPT:
      return 1;
    case FUSE_OPT_KEY_NONOPT:
      return 1;
    case KEY_HELP:
      usage(NULL);
      exit(1);
    default:
      assert(0);
      exit(1);
  }
}
bool done = false;





void my_fuse_session_loop(struct fuse_session *se) {
  // Registering the signal handler


  printf("in session loop\n");
  int res = 0;
  while (!fuse_session_exited(se) ) {
    struct fuse_buf fbuf{.mem= NULL};
    //printf("Before receive_buf %lu \n",pthread_self());
    res = fuse_session_receive_buf(se, &fbuf);


    //sleep(2);
    if (res == -EINTR) {
      free(fbuf.mem);
      //free(fbuf);
      continue;
    }
    if (res <= 0) {
      free(fbuf.mem);
      //free(fbuf);
      break;
    }

    when() <<[=](){
      fuse_session_process_buf(se, &fbuf);
    };
  }
  std::cout << "out of the loop " << std::endl;
}





class filesystem_base {
public:
  filesystem_base() {
    std::memset(&ops_, 0, sizeof(ops_));
    ops_.init = ll_init,
    ops_.create = ll_create;
    ops_.release = ll_release;
    ops_.unlink = ll_unlink;
    ops_.forget = ll_forget;
    //ops_.forget_multi = ll_forget_multi;
    ops_.getattr = ll_getattr;
    ops_.lookup = ll_lookup;
    ops_.opendir = ll_opendir;
    ops_.readdir = ll_readdir;
    //ops_.releasedir = ll_releasedir;
    ops_.open = ll_open;
    ops_.write = ll_write;
    ops_.read = ll_read;

    ops_.mkdir = ll_mkdir;
    ops_.rmdir = ll_rmdir;
    ops_.rename = ll_rename;
    ops_.setattr = ll_setattr;
    //ops_.readlink = ll_readlink;
    //ops_.symlink = ll_symlink;
    //ops_.fsync = ll_fsync;
    //ops_.fsyncdir = ll_fsyncdir;
    //ops_.statfs = ll_statfs;
    //ops_.link = ll_link;
    ops_.access = ll_access;
    //ops_.mknod = ll_mknod;
    ops_.fallocate = ll_fallocate;
  }



public:
  //virtual void destroy() = 0;
  virtual int lookup(fuse_ino_t parent_ino, const std::string& name,fuse_req_t req) = 0;
  virtual void forget(fuse_ino_t ino, long unsigned nlookup,fuse_req_t req) = 0;
  //virtual int statfs(fuse_ino_t ino, struct statvfs* stbuf) = 0;
  virtual void init(void *userdata,struct fuse_conn_info *conn) = 0;
  //virtual int mknod(fuse_ino_t parent_ino,const std::string& name,mode_t mode,dev_t rdev,struct stat* st,uid_t uid,gid_t gid)= 0;

  //virtual int symlink(const std::string& link,fuse_ino_t parent_ino,const std::string& name,struct stat* st,uid_t uid,gid_t gid)= 0;

  //virtual int link(
  //  fuse_ino_t ino,
  //  fuse_ino_t newparent_ino,
  //  const std::string& newname,
  //  struct stat* st,
  //  uid_t uid,
  //  gid_t gid)
  //  = 0;
  //
  virtual int rename(
    fuse_ino_t parent_ino,
    const std::string& name,
    fuse_ino_t newparent_ino,
    const std::string& newname,
    uid_t uid,
    gid_t gid,fuse_req_t req)
    = 0;

  virtual int unlink(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid,fuse_req_t req) = 0;
  //
  virtual int access(fuse_ino_t ino, int mask, uid_t uid, gid_t gid, fuse_req_t req) = 0;

  virtual int getattr(fuse_ino_t ino, uid_t uid, gid_t gid,fuse_req_t req,int *ptr) = 0;

  virtual int setattr(fuse_ino_t ino,FileHandle* fh,struct stat* attr,int to_set,uid_t uid,gid_t gid,fuse_req_t req)= 0;

  //virtual ssize_t
  //readlink(fuse_ino_t ino, char* path, size_t maxlen, uid_t uid, gid_t gid)
  //  = 0;

  virtual int mkdir(fuse_ino_t parent_ino,const std::string& name,mode_t mode,uid_t uid,gid_t gid, fuse_req_t req)= 0;

  //virtual int opendir(fuse_ino_t ino, int flags, uid_t uid, gid_t gid) = 0;
  //
  virtual ssize_t readdir(fuse_req_t req, fuse_ino_t ino, size_t bufsize, off_t off)= 0;
  //
  virtual int rmdir(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid,fuse_req_t req) = 0;

  virtual int opendir(fuse_ino_t ino, int flags, uid_t uid, gid_t gid,fuse_req_t req,struct fuse_file_info *fi) = 0;
  //virtual void releasedir(fuse_ino_t ino) = 0;

  virtual void my_fallocate(fuse_req_t req, fuse_ino_t ino, int mode,off_t offset, off_t length, fuse_file_info *fi) = 0;

  virtual int create(fuse_ino_t parent_ino,const std::string& name,mode_t mode,int flags,uid_t uid,gid_t gid,fuse_req_t req,struct fuse_file_info* fi)= 0;

  virtual int open(fuse_ino_t ino, int flags, FileHandle** fhp, uid_t uid, gid_t gid,  struct fuse_file_info* fi,fuse_req_t req) = 0;
  virtual ssize_t write(FileHandle* fh, const char * buf, size_t size, off_t off, struct fuse_file_info *fi,fuse_req_t req, int *ptr, fuse_ino_t ino)= 0;
  virtual ssize_t read(FileHandle* fh, off_t offset, size_t size, fuse_req_t req,fuse_ino_t ino) = 0;
  virtual void release(fuse_ino_t ino, FileHandle* fh) = 0;

private:
  static filesystem_base* get(fuse_req_t req) {
    return get(fuse_req_userdata(req));
  }

  static filesystem_base* get(void* userdata) {
    return reinterpret_cast<filesystem_base*>(userdata);
  }

  //static void ll_destroy(void* userdata) {
  //  auto fs = get(userdata);
  //  fs->destroy();
  //}

  static void ll_init(void *userdata,
                      struct fuse_conn_info *conn)
  {
    conn->want &= ~FUSE_CAP_AUTO_INVAL_DATA;


    if(conn->capable & FUSE_CAP_PARALLEL_DIROPS)
      conn->want |= FUSE_CAP_PARALLEL_DIROPS;

    if(conn->capable & FUSE_CAP_ASYNC_READ)
      conn->want |= FUSE_CAP_ASYNC_READ;

    if (conn->capable & FUSE_CAP_WRITEBACK_CACHE)
      conn->want |= FUSE_CAP_WRITEBACK_CACHE;

    if(conn->capable & FUSE_CAP_ASYNC_DIO)
      conn->want |= FUSE_CAP_ASYNC_DIO;

    if (conn->capable & FUSE_CAP_SPLICE_WRITE)
      conn->want |= FUSE_CAP_SPLICE_WRITE;

    if (conn->capable & FUSE_CAP_SPLICE_READ)
      conn->want |= FUSE_CAP_SPLICE_READ;

    auto fs = get(userdata);
    fs->init(userdata,conn);
  }



  static void ll_create(
    fuse_req_t req,
    fuse_ino_t parent,
    const char* name,
    mode_t mode,
    struct fuse_file_info* fi) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    fs->create(parent, name, mode, fi->flags, ctx->uid, ctx->gid,req,fi);
  }



  static void
  ll_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = get(req);
    auto fh = reinterpret_cast<FileHandle*>(fi->fh);

    fs->release(ino, fh);
    fuse_reply_err(req, 0);
  }

  static void ll_unlink(fuse_req_t req, fuse_ino_t parent, const char* name) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    fs->unlink(parent, name, ctx->uid, ctx->gid,req);
  }

  static void
  ll_forget(fuse_req_t req, fuse_ino_t ino, long unsigned nlookup) {
    auto fs = get(req);

    fs->forget(ino, nlookup,req);
    //fuse_reply_none(req);
  }

  //static void ll_forget_multi(
  //  fuse_req_t req, size_t count, struct fuse_forget_data* forgets) {
  //  auto fs = get(req);
  //
  //  for (size_t i = 0; i < count; i++) {
  //    const struct fuse_forget_data* f = forgets + i;
  //    fs->forget(f->ino, f->nlookup);
  //  }
  //
  //  fuse_reply_none(req);
  //}

  static void reply_getattr_success(fuse_req_t req,struct stat st,int code){

  }

  static void
  ll_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);


    fs->getattr(ino, ctx->uid, ctx->gid,req,req->buf_mem);
  }





  static void ll_lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
    auto fs = get(req);
    //printf("in lookup\n");
    fs->lookup(parent, name,req);
  }

  static void
  ll_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    //printf("Ptr is %p \n",req->buf_mem);
    fs->opendir(ino, fi->flags, ctx->uid, ctx->gid,req,fi);
    //if (ret == 0) {
    //fuse_reply_open(req, fi);
    //} else {
    //  fuse_reply_err(req, -ret);
    //}
  }



  static void ll_readdir(
    fuse_req_t req,
    fuse_ino_t ino,
    size_t size,
    off_t off,
    struct fuse_file_info* fi) {
    auto fs = get(req);

    //printf("in readdir, thread %lu \n",pthread_self());


    fs->readdir(req, ino,  size, off);
  }

  //static void
  //ll_releasedir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
  //  auto fs = get(req);
  //
  //  fs->releasedir(ino);
  //  fuse_reply_err(req, 0);
  //}
  //
  static void ll_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    // new files are handled by ll_create
    assert(!(fi->flags & O_CREAT));
    FileHandle* fh;
    fs->open(ino, fi->flags, &fh, ctx->uid, ctx->gid,fi,req);

    //if (ret == 0) {
    //  fi->fh = reinterpret_cast<uint64_t>(fh);
    //  fuse_reply_open(req, fi);
    //} else {
    //  fuse_reply_err(req, -ret);
    //}
  }



  static void ll_write (fuse_req_t req, fuse_ino_t ino, const char *buf,size_t size, off_t off, struct fuse_file_info *fi) {

    auto fs = get(req);
    auto fh = reinterpret_cast<FileHandle*>(fi->fh);

    //std::cout << "I am thread in write" << pthread_self() <<  "for ino " << ino << std::endl;

    //printf("in write for ino %lu and size %lu \n",ino,size);
    //while (1 );
    //count++;
    //printf("seeing memory at %d\n",*req->buf_mem);
    //printf("Address in write %lu\n",fi->fh);
    fs->write(fh,buf,size,off,fi,req,req->buf_mem,ino);
    //TODO REMOVE AND CHANGE
    //if (ret >= 0)
    //  fuse_reply_write(req, ret);
    //else
    //  fuse_reply_err(req, -ret);
  }

  static void ll_read(
    fuse_req_t req,
    fuse_ino_t ino,
    size_t size,
    off_t off,
    struct fuse_file_info* fi) {
    auto fs = get(req);
    auto fh = reinterpret_cast<FileHandle*>(fi->fh);
    fs->read(fh, off, size, req, ino);
    //TODO: REMOVE IT FROM HERE AND PLACE IT IN THE CORRECT POSITION
    //if (ret >= 0)
    //  fuse_reply_buf(req, buf.get(), ret);
    //else
    //  fuse_reply_err(req, -ret);
  }



  static void
  ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    fs->mkdir(parent, name, mode, ctx->uid, ctx->gid,req);
  }

  static void ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char* name) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    fs->rmdir(parent, name, ctx->uid, ctx->gid,req);
    //fuse_reply_err(req, -ret);
  }

  static void ll_rename(
    fuse_req_t req,
    fuse_ino_t parent,
    const char* name,
    fuse_ino_t newparent,
    const char* newname,unsigned int idk) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);

    fs->rename(parent, name, newparent, newname, ctx->uid, ctx->gid,req);
    //fuse_reply_err(req, -ret);
  }

  static void ll_setattr(
    fuse_req_t req,
    fuse_ino_t ino,
    struct stat* attr,
    int to_set,
    struct fuse_file_info* fi) {
    auto fs = get(req);
    auto fh = fi ? reinterpret_cast<FileHandle*>(fi->fh) : nullptr;
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    fs->setattr(ino, fh, attr, to_set, ctx->uid, ctx->gid,req);
  }

  //static void ll_readlink(fuse_req_t req, fuse_ino_t ino) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //  char path[PATH_MAX + 1];
  //
  //  ssize_t ret = fs->readlink(
  //    ino, path, sizeof(path) - 1, ctx->uid, ctx->gid);
  //  if (ret >= 0) {
  //    path[ret] = '\0';
  //    fuse_reply_readlink(req, path);
  //  } else {
  //    int r = (int)ret;
  //    fuse_reply_err(req, -r);
  //  }
  //}
  //
  //static void ll_symlink(
  //  fuse_req_t req, const char* link, fuse_ino_t parent, const char* name) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //
  //  struct fuse_entry_param fe;
  //  std::memset(&fe, 0, sizeof(fe));
  //
  //  int ret = fs->symlink(link, parent, name, &fe.attr, ctx->uid, ctx->gid);
  //  if (ret == 0) {
  //    fe.ino = fe.attr.st_ino;
  //    fuse_reply_entry(req, &fe);
  //  } else {
  //    fuse_reply_err(req, -ret);
  //  }
  //}
  //
  //static void ll_fsync(
  //  fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info* fi) {
  //  fuse_reply_err(req, 0);
  //}
  //
  //static void ll_fsyncdir(
  //  fuse_req_t req, fuse_ino_t ino, int datasync, struct fuse_file_info* fi) {
  //  fuse_reply_err(req, 0);
  //}
  //
  //static void ll_statfs(fuse_req_t req, fuse_ino_t ino) {
  //  auto fs = get(req);
  //
  //  struct statvfs stbuf;
  //  std::memset(&stbuf, 0, sizeof(stbuf));
  //
  //  int ret = fs->statfs(ino, &stbuf);
  //  if (ret == 0)
  //    fuse_reply_statfs(req, &stbuf);
  //  else
  //    fuse_reply_err(req, -ret);
  //}
  //
  //static void ll_link(
  //  fuse_req_t req,
  //  fuse_ino_t ino,
  //  fuse_ino_t newparent,
  //  const char* newname) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //
  //  struct fuse_entry_param fe;
  //  std::memset(&fe, 0, sizeof(fe));
  //
  //  int ret = fs->link(
  //    ino, newparent, newname, &fe.attr, ctx->uid, ctx->gid);
  //  if (ret == 0) {
  //    fe.ino = fe.attr.st_ino;
  //    fuse_reply_entry(req, &fe);
  //  } else {
  //    fuse_reply_err(req, -ret);
  //  }
  //}
  //
  static void ll_access(fuse_req_t req, fuse_ino_t ino, int mask) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    fs->access(ino, mask, ctx->uid, ctx->gid, req);
  }

  //static void ll_mknod(
  //  fuse_req_t req,
  //  fuse_ino_t parent,
  //  const char* name,
  //  mode_t mode,
  //  dev_t rdev) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //  printf("shitttt\n");
  //  exit(-99);
  //  struct fuse_entry_param fe;
  //  std::memset(&fe, 0, sizeof(fe));
  //
  //  int ret = fs->mknod(
  //    parent, name, mode, rdev, &fe.attr, ctx->uid, ctx->gid);
  //  if (ret == 0) {
  //    fe.ino = fe.attr.st_ino;
  //    fuse_reply_entry(req, &fe);
  //  } else {
  //    fuse_reply_err(req, -ret);
  //  }
  //}

  //TODO IMPLEMENT FALLOCATE
  static void ll_fallocate(fuse_req_t req,fuse_ino_t ino,int mode,off_t offset,off_t length,struct fuse_file_info* fi) {
    auto fs = get(req);
    fs->my_fallocate(req,ino,mode,offset,length,fi);
  }

protected:
  fuse_lowlevel_ops ops_;
  static void reply_fail(int ret,fuse_req_t req){
    fuse_reply_err(req, -ret);
  }


};


static void reply_create_success(const struct fuse_file_info* fi,FileHandle* fh,fuse_req_t req,struct stat st){
  struct fuse_entry_param fe;
  std::memset(&fe, 0, sizeof(fe));

  fe.attr = st;
  fe.ino = fe.attr.st_ino;
  fe.generation = 0;
  fe.entry_timeout = 1.0;
  fuse_reply_create(req, &fe, fi);
}

static void reply_lookup_success(struct fuse_entry_param fe,fuse_req_t req){
  fe.ino = fe.attr.st_ino;
  fe.generation = 0;
  fuse_reply_entry(req, &fe);
}



static void reply_mkdir(struct stat st,fuse_req_t req){
  struct fuse_entry_param fe;
  std::memset(&fe, 0, sizeof(fe));
  fe.attr = st;
  fe.ino = fe.attr.st_ino;
  fe.generation = 0;
  fe.entry_timeout = 1.0;
  fuse_reply_entry(req, &fe);
}



static void reply_read(int ret,fuse_req_t req,char *buf){
  if (ret >= 0)
    fuse_reply_buf(req, buf, ret);
  else
    fuse_reply_err(req, -ret);
  free(buf);
}