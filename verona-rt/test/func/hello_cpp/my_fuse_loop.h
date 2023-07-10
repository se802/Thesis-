//
// Created by csdeptucy on 7/8/23.
//

#ifndef VERONA_RT_ALL_MY_FUSE_LOOP_H
#define VERONA_RT_ALL_MY_FUSE_LOOP_H

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fuse3/fuse_lowlevel.h>
#include <unistd.h>
#include "fuse3/fuse_i.h"


struct fuse_in_header {
  uint32_t	len;
  uint32_t	opcode;
  uint64_t	unique;
  uint64_t	nodeid;
  uint32_t	uid;
  uint32_t	gid;
  uint32_t	pid;
  uint16_t	total_extlen; /* length of extensions in 8byte units */
  uint16_t	padding;
};
//
//struct fuse_chan {
//  pthread_mutex_t lock;
//  int ctr;
//  int fd;
//};

int my_fuse_session_receive_buf_int(struct fuse_session *se, struct fuse_buf *buf,
                                 struct fuse_chan *ch)
{
  int err;
  ssize_t res;
  if (!buf->mem) {
    buf->mem = malloc(se->bufsize);
    if (!buf->mem) {
      fuse_log(FUSE_LOG_ERR,
               "fuse: failedd to allocate read buffer\n");
      return -ENOMEM;
    }
  }

restart:
  if (se->io != NULL) {
    /* se->io->read is never NULL if se->io is not NULL as
    specified by fuse_session_custom_io()*/
    res = se->io->read(ch ? ch->fd : se->fd, buf->mem, se->bufsize,
                       se->userdata);
  } else {
    res = read(ch ? ch->fd : se->fd, buf->mem, se->bufsize);
  }
  err = errno;

  if (fuse_session_exited(se))
    return 0;
  if (res == -1) {
    /* ENOENT means the operation was interrupted, it's safe
       to restart */
    if (err == ENOENT)
      goto restart;

    if (err == ENODEV) {
      /* Filesystem was unmounted, or connection was aborted
         via /sys/fs/fuse/connections */
      fuse_session_exit(se);
      return 0;
    }
    /* Errors occurring during normal operation: EINTR (read
       interrupted), EAGAIN (nonblocking I/O), ENODEV (filesystem
       umounted) */
    if (err != EINTR && err != EAGAIN)
      perror("fuse: reading device");
    return -err;
  }
  if ((size_t) res < sizeof(struct fuse_in_header)) {
    fuse_log(FUSE_LOG_ERR, "short read on fuse device\n");
    return -EIO;
  }

  buf->size = res;
  return res;
}



int my_fuse_session_loop(struct fuse_session *se)
{
  int res = 0;
  struct fuse_buf fbuf = {
    .mem = NULL,
  };

  while (!fuse_session_exited(se)) {
    res = my_fuse_session_receive_buf_int(se, &fbuf,NULL);

    if (res == -EINTR)
      continue;
    if (res <= 0)
      break;

    fuse_session_process_buf(se, &fbuf);
  }

  free(fbuf.mem);
  if(res > 0)
    /* No error, just the length of the most recently read
       request */
    res = 0;
  if(se->error != 0)
    res = se->error;
  fuse_session_reset(se);
  return res;
}



#endif // VERONA_RT_ALL_MY_FUSE_LOOP_H
