
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
#include "fuse.h"
#include "pthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

static  char hello_str[50] = "Hello World!!!!\n";
static  char hello_name[50] = "hello";

static int hello_stat(fuse_ino_t ino, struct stat *stbuf)
{

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

        default:
            return -1;
    }
    return 0;
}

static void hello_ll_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi)
{
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
    printf("Looking for inode of %s\n",name);
    struct fuse_entry_param e;

    if (parent != 1 || strcmp(name, hello_name) != 0)
        fuse_reply_err(req, ENOENT);
    else {
        memset(&e, 0, sizeof(e));
        e.ino = 2;
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
    fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
                      b->size);
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
    printf("Process: %d, Thread: %lud in readdir\n",pid,tid);
    (void) fi;

    if (ino != 1)
        fuse_reply_err(req, ENOTDIR);
    else {
        struct dirbuf b;

        memset(&b, 0, sizeof(b));
        dirbuf_add(req, &b, ".", 1);
        dirbuf_add(req, &b, "..", 1);
        dirbuf_add(req, &b, hello_name, 2);
        reply_buf_limited(req, b.p, b.size, off, size);
        free(b.p);
    }
}

static void hello_ll_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi)
{
    printf("in open \n");
    if (ino != 2)
        fuse_reply_err(req, EISDIR);
    else
        fuse_reply_open(req, fi);
}

static void hello_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi)
{
    printf("in read %d \n",ino);
    (void) fi;
    if(ino==2)
        reply_buf_limited(req, hello_str, strlen(hello_str), off, size);
}



static void hello_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("write\n");
    if (ino != 2) {
        fuse_reply_err(req, ENOENT);
        return;
    }


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
}

static const struct fuse_lowlevel_ops hello_ll_oper = {
        .lookup		= hello_ll_lookup,
        .getattr	= hello_ll_getattr,
        .readdir	= hello_ll_readdir,
        .open		= hello_ll_open,
        .read		= hello_ll_read,
        .write      = hello_ll_write
};

int main(int argc, char *argv[])
{
  
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
        goto err_out1;
    } else if (opts.show_version) {
        printf("FUSE library version %s\n", fuse_pkgversion());
        fuse_lowlevel_version();
        ret = 0;
        goto err_out1;
    }

    if(opts.mountpoint == NULL) {
        printf("usage: %s [options] <mountpoint>\n", argv[0]);
        printf("       %s --help\n", argv[0]);
        ret = 1;
        goto err_out1;
    }

    se = fuse_session_new(&args, &hello_ll_oper,
                          sizeof(hello_ll_oper), NULL);
    if (se == NULL)
        goto err_out1;

    if (fuse_set_signal_handlers(se) != 0)
        goto err_out2;

    if (fuse_session_mount(se, opts.mountpoint) != 0)
        goto err_out3;

    fuse_daemonize(opts.foreground);

    /* Block until ctrl+c or fusermount -u */
    if (opts.singlethread)
    {
        ret = fuse_session_loop(se);
        printf("SingleThread\n");
    }
    else {
        printf("MultiThread\n");
        config.clone_fd = opts.clone_fd;
        config.max_idle_threads = opts.max_idle_threads;
        ret = fuse_session_loop_mt(se, &config);
    }
    printf("Hello\n");

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