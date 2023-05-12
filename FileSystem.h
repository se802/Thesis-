#define FUSE_USE_VERSION 34

#include <bitset>
#include <cpp/when.h>
#include <cstdint>
#include <cstring>
#include <fuse3/fuse_lowlevel.h>
#include <memory>
#include <sys/stat.h>
#include <vector>

#define DIRECT_POINTERS 11
#define NUM_INODES 1024
#define NUM_DATABLOCKS 4096
#define BLOCK_SIZE 4096

using namespace verona::cpp;

class FileSystem;

class InodeBitmap {
public:
  size_t find_free_inode();
  void release_inode(size_t inode_index);

private:
  std::bitset<NUM_INODES> bitset;
};

class BlockBitmap {
public:
  size_t find_free_block();
  void set_block_free(size_t block_index);

private:
  std::bitset<NUM_DATABLOCKS> bitset;
};

class Block{
public:
  Block();
  void read_block(char* buffer, size_t size, int offset);
  void write_block(char* buffer, size_t size, int offset);


  char buf[BLOCK_SIZE];
};



class Inode {
public:
  Inode(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode,FileSystem *fs);
  virtual ~Inode() = 0;
  struct stat inode_metadata; // 144 bytes
  uint32_t direct_pointers[DIRECT_POINTERS]; // 11 Direct pointers - 11 * 32 =  352 bytes

private:
  char pad[4]; // 4 bytes for pad -- Inode size = 512 bytes
protected:
  // 8 Bytes pointer -- Inode size = 512 bytes
  FileSystem *fs;
};

class RegInode : public Inode {
public:
  RegInode(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode, FileSystem* fs);
  ~RegInode();

  off_t write(char *buf, int bytes_to_write, int offset, FileSystem* fs);
  int read(char *buf, int bytes_to_read, int offset, FileSystem* fs);


};

/**
 * Discuss about the design of DirInode
 * Current Design:
 *  - DirInodes will be cowns
 *  - addition/deletion of directory entries will be available only through DirInode interface
 *  - i.e when(dir1,dir2) << { dir1.add_entry(dentry); dir2.remove_entry(dentry) };
 */
class DirInode : public Inode  {
public:
  struct DirEntry {
    uint32_t ino; // inode number
    char name[224]; // File name, null-terminated
  };

  DirInode(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode, FileSystem* fs);
  ~DirInode();

  bool add_entry(const std::string& name, uint32_t ino);
  bool remove_entry(const std::string& name, uint32_t ino);

private:
  static constexpr size_t ENTRIES_PER_BLOCK = BLOCK_SIZE / sizeof (DirEntry);
};

class FileSystem {
public:

  FileSystem();
  int find_free_data_block();
  void release_data_block(uint32_t block_idx);

  int find_free_inode();
  void release_inode(uint32_t inode_idx);

  cown_ptr<Block> getDataBlock(int idx){
    return data_blocks[idx];
  }

  std::shared_ptr<RegInode> getRegularInode(int node_id);
  cown_ptr<DirInode> getDirInode(int node_id);

private:

  cown_ptr<InodeBitmap> inode_bitmap;
  cown_ptr<BlockBitmap> block_bitmap;
  std::unordered_map<int, std::shared_ptr<RegInode>> regular_inode_table;
  std::unordered_map<int, cown_ptr<DirInode>> directory_inode_table;
  std::vector<cown_ptr<Block>> data_blocks;
};



