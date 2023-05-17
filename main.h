#pragma once

#include "debug/harness.h"
#include "cpp/when.h"

#include <atomic>
#include <cassert>
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

struct my_thread_data {
  struct fuse_session *se;
  struct fuse_buf *fbuf;
};

void *my_thread_function(void *arg) {
  struct my_thread_data *data = (struct my_thread_data *) arg;
  struct fuse_session *se = data->se;
  struct fuse_buf *fbuf = data->fbuf;

  int sec = rand() % 500;
  // printf("I am thread %lu and I am going to sleep %d nanoseconds\n",pthread_self(),sec);
  //usleep(sec);

  // process the FUSE request
  fuse_session_process_buf(se, fbuf);
  //free(fbuf->mem);
  //free(fbuf);
  //free(data);

  return NULL;
}







struct ExternalSource;




struct Poller : VCown<Poller>
{
  std::shared_ptr<ExternalSource> es;
};

struct ExternalSource
{
  Poller* p;
  std::atomic<bool> notifications_on;
  Notification* n;

  ExternalSource(Poller* p_) : p(p_), notifications_on(false)
  {
    Cown::acquire(p);
  }

  ~ExternalSource()
  {
    Logging::cout() << "~ExternalSource" << Logging::endl;
  }
  void test(){
    printf("test\n");
  }

  void my_fuse_session_loop(struct fuse_session *se) {

    int res = 0;
    while (!fuse_session_exited(se)) {
      struct fuse_buf *fbuf = (struct fuse_buf *) malloc(sizeof(struct fuse_buf));
      fbuf->mem = NULL;
      if (fbuf == NULL) {
        // handle allocation failure
        break;
      }
      //printf("Before receive_buf %lu \n",pthread_self());
      res = fuse_session_receive_buf(se, fbuf);

      if (res == -EINTR) {
        free(fbuf->mem);
        free(fbuf);
        continue;
      }
      if (res <= 0) {
        free(fbuf->mem);
        free(fbuf);
        break;
      }








      when() << [=]() {
        fuse_session_process_buf(se, fbuf);
        free(fbuf);
      };
    }


  }







};

void my_session_loop(SystematicTestHarness* harness,struct fuse_session *se)
{
  auto& alloc = ThreadAlloc::get();
  auto* p = new (alloc) Poller();
  auto es = std::make_shared<ExternalSource>(p);


  schedule_lambda<YesTransfer>(p, [=]() {
    // Start IO Thread
    Scheduler::add_external_event_source();
    harness->external_thread([=]() {   es->my_fuse_session_loop(se); });
  });
}


class filesystem_base {
public:
  filesystem_base() {
    std::memset(&ops_, 0, sizeof(ops_));
    ops_.init = ll_init,
    //ops_.destroy = ll_destroy;
      ops_.create = ll_create;
    //ops_.release = ll_release;
    //ops_.unlink = ll_unlink;
    //ops_.forget = ll_forget;
    //ops_.forget_multi = ll_forget_multi;
    ops_.getattr = ll_getattr;
    ops_.lookup = ll_lookup;
    //ops_.opendir = ll_opendir;
    ops_.readdir = ll_readdir;
    //ops_.releasedir = ll_releasedir;
    //ops_.open = ll_open;
    ops_.write = ll_write;
    //ops_.read = ll_read;
    ops_.mkdir = ll_mkdir;
    //ops_.rmdir = ll_rmdir;
    //ops_.rename = ll_rename;
    ops_.setattr = ll_setattr;
    //ops_.readlink = ll_readlink;
    //ops_.symlink = ll_symlink;
    //ops_.fsync = ll_fsync;
    //ops_.fsyncdir = ll_fsyncdir;
    //ops_.statfs = ll_statfs;
    //ops_.link = ll_link;
    ops_.access = ll_access;
    //ops_.mknod = ll_mknod;
    //ops_.fallocate = ll_fallocate;
  }



public:
  //virtual void destroy() = 0;
  virtual int lookup(fuse_ino_t parent_ino, const std::string& name,fuse_req_t req) = 0;
  //virtual void forget(fuse_ino_t ino, long unsigned nlookup) = 0;
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
  //virtual int rename(
  //  fuse_ino_t parent_ino,
  //  const std::string& name,
  //  fuse_ino_t newparent_ino,
  //  const std::string& newname,
  //  uid_t uid,
  //  gid_t gid)
  //  = 0;
  //
  //virtual int
  //unlink(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid)
  //  = 0;
  //
  virtual int access(fuse_ino_t ino, int mask, uid_t uid, gid_t gid, fuse_req_t req) = 0;

  virtual int getattr(fuse_ino_t ino, uid_t uid, gid_t gid,fuse_req_t req) = 0;

  virtual int setattr(fuse_ino_t ino,FileHandle* fh,struct stat* attr,int to_set,uid_t uid,gid_t gid,fuse_req_t req)= 0;

  //virtual ssize_t
  //readlink(fuse_ino_t ino, char* path, size_t maxlen, uid_t uid, gid_t gid)
  //  = 0;

  virtual int mkdir(fuse_ino_t parent_ino,const std::string& name,mode_t mode,uid_t uid,gid_t gid, fuse_req_t req)= 0;

  //virtual int opendir(fuse_ino_t ino, int flags, uid_t uid, gid_t gid) = 0;
  //
  virtual ssize_t readdir(fuse_req_t req, fuse_ino_t ino, size_t bufsize, off_t off)= 0;
  //
  //virtual int
  //rmdir(fuse_ino_t parent_ino, const std::string& name, uid_t uid, gid_t gid)
  //  = 0;
  //
  //virtual void releasedir(fuse_ino_t ino) = 0;

  virtual int create(fuse_ino_t parent_ino,const std::string& name,mode_t mode,int flags,uid_t uid,gid_t gid,fuse_req_t req,struct fuse_file_info* fi)= 0;

  virtual int open(fuse_ino_t ino, int flags, FileHandle** fhp, uid_t uid, gid_t gid,  struct fuse_file_info* fi,fuse_req_t req) = 0;
  //virtual ssize_t write(FileHandle* fh, const char * buf, size_t size, off_t off, struct fuse_file_info *fi,fuse_req_t req)= 0;
  //virtual ssize_t read(FileHandle* fh, off_t offset, size_t size, char* buf)
  //  = 0;
  //virtual void release(fuse_ino_t ino, FileHandle* fh) = 0;

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

  //static void
  //ll_release(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
  //  auto fs = get(req);
  //  auto fh = reinterpret_cast<FileHandle*>(fi->fh);
  //
  //  fs->release(ino, fh);
  //  fuse_reply_err(req, 0);
  //}

  //static void ll_unlink(fuse_req_t req, fuse_ino_t parent, const char* name) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //
  //  int ret = fs->unlink(parent, name, ctx->uid, ctx->gid);
  //  fuse_reply_err(req, -ret);
  //}

  //static void
  //ll_forget(fuse_req_t req, fuse_ino_t ino, long unsigned nlookup) {
  //  auto fs = get(req);
  //
  //  fs->forget(ino, nlookup);
  //  fuse_reply_none(req);
  //}
  //
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
    struct stat st;
    fs->getattr(ino, ctx->uid, ctx->gid,req);
    //if (ret == 0)
    //  fuse_reply_attr(req, &st, ret);
    //else
    //  fuse_reply_err(req, -ret);
  }





  static void ll_lookup(fuse_req_t req, fuse_ino_t parent, const char* name) {
    auto fs = get(req);
    fs->lookup(parent, name,req);
  }

  //static void
  //ll_opendir(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info* fi) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //
  //  int ret = fs->opendir(ino, fi->flags, ctx->uid, ctx->gid);
  //  if (ret == 0) {
  //    fuse_reply_open(req, fi);
  //  } else {
  //    fuse_reply_err(req, -ret);
  //  }
  //}



  static void ll_readdir(
    fuse_req_t req,
    fuse_ino_t ino,
    size_t size,
    off_t off,
    struct fuse_file_info* fi) {
    auto fs = get(req);


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

    //fs->write(fh,buf,size,off,fi,req);
    //TODO REMOVE AND CHANGE
    //if (ret >= 0)
    //  fuse_reply_write(req, ret);
    //else
    //  fuse_reply_err(req, -ret);
  }

  //static void ll_read(
  //  fuse_req_t req,
  //  fuse_ino_t ino,
  //  size_t size,
  //  off_t off,
  //  struct fuse_file_info* fi) {
  //  auto fs = get(req);
  //  auto fh = reinterpret_cast<FileHandle*>(fi->fh);
  //
  //  auto buf = std::unique_ptr<char[]>(new char[size]);
  //
  //  ssize_t ret = fs->read(fh, off, size, buf.get());
  //  if (ret >= 0)
  //    fuse_reply_buf(req, buf.get(), ret);
  //  else
  //    fuse_reply_err(req, -ret);
  //}



  static void
  ll_mkdir(fuse_req_t req, fuse_ino_t parent, const char* name, mode_t mode) {
    auto fs = get(req);
    const struct fuse_ctx* ctx = fuse_req_ctx(req);
    fs->mkdir(parent, name, mode, ctx->uid, ctx->gid,req);
  }

  //static void ll_rmdir(fuse_req_t req, fuse_ino_t parent, const char* name) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //
  //  int ret = fs->rmdir(parent, name, ctx->uid, ctx->gid);
  //  fuse_reply_err(req, -ret);
  //}
  //
  //static void ll_rename(
  //  fuse_req_t req,
  //  fuse_ino_t parent,
  //  const char* name,
  //  fuse_ino_t newparent,
  //  const char* newname,unsigned int idk) {
  //  auto fs = get(req);
  //  const struct fuse_ctx* ctx = fuse_req_ctx(req);
  //
  //  int ret = fs->rename(
  //    parent, name, newparent, newname, ctx->uid, ctx->gid);
  //  fuse_reply_err(req, -ret);
  //}
  //
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

  static void ll_fallocate(
    fuse_req_t req,
    fuse_ino_t ino,
    int mode,
    off_t offset,
    off_t length,
    struct fuse_file_info* fi) {
    fuse_reply_err(req, 0);
  }

protected:
  fuse_lowlevel_ops ops_;
  static void reply_fail(int ret,fuse_req_t req){
    fuse_reply_err(req, -ret);
  }

  static void reply_create_success(struct fuse_file_info* fi,FileHandle* fh,fuse_req_t req,struct stat st){
    struct fuse_entry_param fe;
    std::memset(&fe, 0, sizeof(fe));

    fi->fh = reinterpret_cast<uint64_t>(fh);
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

  static void reply_readdir(fuse_req_t req,char *buf,size_t ret){
    if (ret >= 0) {
      fuse_reply_buf(req, buf, (size_t)ret);
    } else {
      //FIXME: ELSE WILL NEVER BE EXECUTED
      int r = (int)ret;
      fuse_reply_err(req, -r);
    }
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

  static void reply_write(size_t ret,fuse_req_t req){
    if (ret >= 0)
      fuse_reply_write(req, ret);
    else
      fuse_reply_err(req, -ret);
  }

};
