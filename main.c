#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>


/**
struct stat {
dev_t     st_dev;
ino_t     st_ino;          Inode number
mode_t    st_mode;         File type and mode
nlink_t   st_nlink;        Number of hard links
uid_t     st_uid;          User ID of owner
gid_t     st_gid;          Group ID of owner
dev_t     st_rdev;         Device ID (if special file)
off_t     st_size;         Total size, in bytes
blksize_t st_blksize;      Block size for filesystem I/O
blkcnt_t  st_blocks;       Number of 512B blocks allocated
*/

// getattr is similar to stat. Returns metadata of the file denoted in path
static int my_getattr( const char *path, struct stat *st ){
    printf("Called my_getattr\n");
    st->st_uid = getuid();
    st->st_gid = getgid();
    // last access time for the file
    st->st_atime = time( NULL );
    // last modification time of the file
    st->st_mtime = time( NULL );

    if ( strcmp( path, "/" ) == 0 )
    {
        st->st_mode = S_IFDIR | 0755;
        // 2 links for: . and ..
        st->st_nlink = 2;
    }
    else
    {
        st->st_mode = S_IFREG | 0644;
        st->st_nlink = 1;
        st->st_size = 1024;
    }
    return 0;
}

static int my_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi )
{
    printf("Called my_readdir\n");
    filler( buffer, ".", NULL, 0 ); // Current Directory
    filler( buffer, "..", NULL, 0 ); // Parent Directory
    if ( strcmp( path, "/" ) == 0 ) // If the user is trying to show the files/directories of the root directory show the following
    {
        filler( buffer, "file54", NULL, 0 );
        filler( buffer, "file349", NULL, 0 );
    }
    return 0;
}

static int my_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi )
{
    printf("Called my_read\n");
    char file54Text[] = "Hello World From File54!";
    char file349Text[] = "Hello World From File349!";
    char *selectedText = NULL;
    if ( strcmp( path, "/file54" ) == 0 )
        selectedText = file54Text;
    else if ( strcmp( path, "/file349" ) == 0 )
        selectedText = file349Text;
    else
        return -1;
    memcpy( buffer, selectedText + offset, size );
    return strlen( selectedText ) - offset;
}

static struct fuse_operations operations = {
        .getattr	= my_getattr,
        .readdir	= my_readdir,
        .read	= my_read,
};

int main(int argc,char **argv) {
    return fuse_main( argc, argv, &operations, NULL );
    return 0;
}
