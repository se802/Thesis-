
/** @file
 *
 * minimal example filesystem using low-level API
 *
 * Compile with:
 *     gcc -Wall hello_ll.c `pkg-config fuse3 --cflags --libs` -o hello_ll
 *     ./hello_ll -f /tmp/ssfs
 */

#define FUSE_USE_VERSION 34

#include <fuse3/fuse_lowlevel.h>
#include "fuse3/fuse.h"

#include "pthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include <cpp/when.h>
#include <debug/harness.h>



using namespace verona::cpp;

static  char hello_str[50] = "Hello World!!!!\n";
static  char hello_name[50] = "hello";

static  char str2[50] = "str2\n";
static  char name2[50] = "str2";

static  char str3[50] = "str3\n";
static  char name3[50] = "str3";

static  char str4[50] = "str4\n";
static  char name4[50] = "str4";

static char tempDir[50]="helloDir";

static char tempDir2[50]="helloDir2";

static int hello_stat(fuse_ino_t ino, struct stat *stbuf)
{
    //printf("in stat for inode%lu\n",ino);
    stbuf->st_ino = ino;
    switch (ino) {
        case 1:
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            break;

        case 2:
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = strlen(hello_str);
            break;
        case 3:
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = strlen(str2);
            break;
        case 4:
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            break;
        case 5:
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = strlen(str3);
            break;
        case 6:
            stbuf->st_mode = S_IFDIR | 0755;
            stbuf->st_nlink = 2;
            break;
        case 7:
            stbuf->st_mode = S_IFREG | 0777;
            stbuf->st_nlink = 1;
            stbuf->st_size = strlen(str4);
            break;
        default:
            return -1;
    }
    return 0;
}

static void hello_ll_getattr(fuse_req_t req, fuse_ino_t ino,struct fuse_file_info *fi)
{
    struct fuse_context *ctx = fuse_get_context();
    ctx->pid;
    //printf("in getattr\n");
    //printf("Thread %lu\n",pthread_self());
    struct stat stbuf;

    (void) fi;

    memset(&stbuf, 0, sizeof(stbuf));
    if (hello_stat(ino, &stbuf) == -1)
        fuse_reply_err(req, ENOENT);
    else
        fuse_reply_attr(req, &stbuf, 1.0);
}

static void hello_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name)
{
    const struct fuse_ctx  *ctx= fuse_req_ctx(req);

    printf("in lookup looking for inode of %s with parent %ld\n",name,parent);
    //printf("Thread %lu\n",pthread_self());
    struct fuse_entry_param e;

    if (parent != 1 && parent!=4 && parent!=6)
        fuse_reply_err(req, ENOENT);
    else if (strcmp(name,hello_name)==0) {
        memset(&e, 0, sizeof(e));
        e.ino = 2;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        hello_stat(e.ino, &e.attr);
        fuse_reply_entry(req, &e);
    } else if(strcmp(name,name2)==0){
        memset(&e, 0, sizeof(e));
        e.ino = 3;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        hello_stat(e.ino, &e.attr);
        fuse_reply_entry(req, &e);
    } else if(strcmp(name,tempDir)==0){
        memset(&e, 0, sizeof(e));
        e.ino = 4;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        hello_stat(e.ino, &e.attr);
        fuse_reply_entry(req, &e);
    } else if(strcmp(name,tempDir2)==0){
        memset(&e, 0, sizeof(e));
        e.ino = 6;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        hello_stat(e.ino, &e.attr);
        fuse_reply_entry(req, &e);
    } else if (strcmp(name,name3)==0){
        memset(&e, 0, sizeof(e));
        e.ino = 5;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        hello_stat(e.ino, &e.attr);
        fuse_reply_entry(req, &e);
    } else if (strcmp(name,name4)==0){
        memset(&e, 0, sizeof(e));
        e.ino = 7;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        hello_stat(e.ino, &e.attr);
        fuse_reply_entry(req, &e);
    }
    else{
        memset(&e, 0, sizeof(e));
        e.ino = 99;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        hello_stat(e.ino, &e.attr);
        fuse_reply_entry(req, &e);
    }
}

struct dirbuf {
    char *p;
    size_t size;
};

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name,
                       fuse_ino_t ino)
{
    struct stat stbuf;
    size_t oldsize = b->size;
    b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
    b->p = (char *) realloc(b->p, b->size);
    memset(&stbuf, 0, sizeof(stbuf));
    stbuf.st_ino = ino;
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))

static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
                             off_t off, size_t maxsize)
{
    if (off < bufsize)
        return fuse_reply_buf(req, buf + off,
                              min(bufsize - off, maxsize));
    else
        return fuse_reply_buf(req, NULL, 0);
}

static void hello_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi)
{

    int pid = getpid();
    pthread_t tid = pthread_self();
    printf("in readdir Process: %d, Thread: %lu for inode %lu\n",pid,tid,ino);
    //while (1);
    //printf("Thread %lu\n",pthread_self());
    (void) fi;

    if(ino==1) {
        struct dirbuf b;
        memset(&b, 0, sizeof(b));
        dirbuf_add(req, &b, ".", 1);
        dirbuf_add(req, &b, "..", 1);
        dirbuf_add(req, &b, hello_name, 2);
        dirbuf_add(req, &b, name2, 3);
        dirbuf_add(req, &b, tempDir, 4);
        reply_buf_limited(req, b.p, b.size, off, size);
        free(b.p);
    } else if(ino==4){
        struct dirbuf b;
        memset(&b, 0, sizeof(b));
        dirbuf_add(req, &b, ".", 1);
        dirbuf_add(req, &b, "..", 1);
        dirbuf_add(req, &b, name3, 5);
        dirbuf_add(req, &b, tempDir2, 6);
        reply_buf_limited(req, b.p, b.size, off, size);
        free(b.p);
    } else if(ino==6){
        struct dirbuf b;
        memset(&b, 0, sizeof(b));
        dirbuf_add(req, &b, ".", 1);
        dirbuf_add(req, &b, "..", 1);
        dirbuf_add(req, &b, name4, 7);
        reply_buf_limited(req, b.p, b.size, off, size);
        free(b.p);
    }
    else {
        fuse_reply_err(req, ENOTDIR);
    }
}

static void hello_ll_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi)
{
    //printf("in open with inode %ld\n",ino);
    //printf("Thread %lu\n",pthread_self());
    if (ino != 2 && ino!=3 && ino!=4 && ino!=5 && ino!=7)
        fuse_reply_err(req, EISDIR);
    else
        fuse_reply_open(req, fi);
}

static void hello_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi)
{
    printf("in read %d \n",ino);
    //while (1);
    //printf("Thread %lu\n",pthread_self());
    (void) fi;
    if(ino==2)
        reply_buf_limited(req, hello_str, strlen(hello_str), off, size);
    else if(ino==3)
        reply_buf_limited(req, str2, strlen(str2), off, size);
    else if(ino==4)
        fuse_reply_err(req, ENOTDIR);
    else if(ino==5)
        reply_buf_limited(req, str3, strlen(str3), off, size);
    else if(ino==7)
        reply_buf_limited(req, str4, strlen(str4), off, size);
}



static void hello_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{

    printf("in write\n");
    //while (1);
    //printf("Thread %lu\n",pthread_self());
    if(ino==4){
        fuse_reply_err(req,ENOTDIR);
        return;
    }
    if (ino != 2 && ino!=3) {
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(ino==2){
        size_t len = strlen(hello_str);

        if (offset > len) {
            fuse_reply_err(req, EFBIG);
            return;
        }

        if (offset + size > sizeof(hello_str)) {
            size = sizeof(hello_str) - offset;
        }

        memcpy(hello_str + offset, buf, size);

        if (offset + size > len) {
            hello_str[offset + size] = '\0';
        }

        fuse_reply_write(req, size);
    } else{
        size_t len = strlen(str2);

        if (offset > len) {
            fuse_reply_err(req, EFBIG);
            return;
        }

        if (offset + size > sizeof(str2)) {
            size = sizeof(str2) - offset;
        }

        memcpy(str2 + offset, buf, size);

        if (offset + size > len) {
            str2[offset + size] = '\0';
        }

        fuse_reply_write(req, size);
    }

}

struct lo_inode {
    struct lo_inode *next; /* protected by lo->mutex */
    struct lo_inode *prev; /* protected by lo->mutex */
    int fd;
    ino_t ino;
    dev_t dev;
    uint64_t refcount; /* protected by lo->mutex */
};

struct lo_data {
    pthread_mutex_t mutex;
    int debug;
    int writeback;
    int flock;
    int xattr;
    char *source;
    double timeout;
    int cache;
    int timeout_set;
    struct lo_inode root; /* protected by lo->mutex */
};

static void lo_init(void *userdata,
                    struct fuse_conn_info *conn)
{

    printf("in init\n");
    printf("want %d\n",conn->want);
    conn->want |= FUSE_CAP_PARALLEL_DIROPS;
    printf("want %d\n",conn->want);
}



static const struct fuse_lowlevel_ops hello_ll_oper = {
        .init       = lo_init,

        .lookup		= hello_ll_lookup,
        .getattr	= hello_ll_getattr,

        .open		= hello_ll_open,

        .read		= hello_ll_read,
        .write      = hello_ll_write,
        .readdir	= hello_ll_readdir,
};

struct my_thread_data {
    struct fuse_session *se;
    struct fuse_buf *fbuf;
    int test =5;
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



class ThreadCown
{
  public:
    void *(*start_routine)(void *);
    void *arg;
    int test =5;

    ThreadCown(void *(*_start_routine)(void *), void *_arg,int val)
    : start_routine(_start_routine), arg(_arg),test(val) {}
};

class Body
{
  public:
    ~Body()
    {
        Logging::cout() << "Body destroyed" << Logging::endl;
    }
};

using namespace verona::cpp;



void create_thread(pthread_t *thread_id, void *(*start_routine)(void *), void *arg)
{
    //pthread_create(thread_id,NULL,start_routine,arg);
    //return ;
    // Create a cown for the operation
    auto thread_cown = make_cown<ThreadCown>( start_routine, arg,35);

    // Execute the operation when thread_cown is available
    when(thread_cown) << [](acquired_cown<ThreadCown> thread_cown) mutable
    {
      pthread_t tid;
      printf("Num %d:\n", thread_cown->test);
      pthread_create(&tid, NULL, thread_cown->start_routine, thread_cown->arg);
    };
}


void my_fuse_session_loop(struct fuse_session *se,SystematicTestHarness &harness) {

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



        // allocate memory for the thread data structure and set its fields
        struct my_thread_data *data = (struct my_thread_data *) malloc(sizeof(struct my_thread_data));
        data->se = se;
        data->fbuf = fbuf;
        data->test = 25;

        // create a new thread and pass the `data` pointer to it
        pthread_t thread_id;

        //create_thread(&thread_id, my_thread_function,(void *)data);
        harness.run(create_thread,&thread_id, my_thread_function,(void *)data);
    }

    if (res > 0) {
        // No error, just the length of the most recently read request
        res = 0;
    }
    //if(se->error != 0)
    //     res = se->error;
    // fuse_session_reset(se);
    //printf("exiting");
    //return res;
}

int main(int argc, char *argv[])
{
    SystematicTestHarness harness(argc, argv);
    int pid = getpid();
    printf("Process id %d\n",pid);
    srand(time(NULL));

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;
    int ret = -1;

    if (fuse_parse_cmdline(&args, &opts) != 0)
        return 1;



    if (opts.show_help) {
        printf("usage: %s [options] <mountpoint>\n\n", argv[0]);
        fuse_cmdline_help();
        fuse_lowlevel_help();
        ret = 0;
        return 0;
        //goto err_out1;
    } else if (opts.show_version) {
        printf("FUSE library version %s\n", fuse_pkgversion());
        fuse_lowlevel_version();
        ret = 0;
        return 0;
        //goto err_out1;
    }

    if(opts.mountpoint == NULL) {
        printf("usage: %s [options] <mountpoint>\n", argv[0]);
        printf("       %s --help\n", argv[0]);
        ret = 1;
        return 0;
        //goto err_out1;
    }

    se = fuse_session_new(&args, &hello_ll_oper,
                          sizeof(hello_ll_oper), NULL);



    if (se == NULL)
        return 0;

    if (fuse_set_signal_handlers(se) != 0)
        return 0;

    if (fuse_session_mount(se, opts.mountpoint) != 0)
        return 0;

    //fuse_daemonize(opts.foreground);

    /* Block until ctrl+c or fusermount -u */

    printf("Single Threaded\n");
    config.clone_fd = opts.clone_fd;
    config.max_idle_threads = opts.max_idle_threads;
    my_fuse_session_loop(se,harness);


    //SystematicTestHarness harness(argc, argv);
    //harness.run(my_fuse_session_loop,se);




    fuse_session_unmount(se);
    err_out3:
    fuse_remove_signal_handlers(se);
    err_out2:
    fuse_session_destroy(se);
    err_out1:
    free(opts.mountpoint);
    fuse_opt_free_args(&args);

    return ret ? 1 : 0;
}