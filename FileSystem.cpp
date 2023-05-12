//
// Created by csdeptucy on 5/11/23.
//

#include "FileSystem.h"

#include <bitset>
#include <iostream>

size_t InodeBitmap::find_free_inode()
{
  auto first = bitset._Find_first();
  bitset.set(first, true);
  return first;
}

void InodeBitmap::release_inode(size_t inode_index)
{
  bitset.set(inode_index, false);
}



size_t BlockBitmap::find_free_block()
{
  auto first = bitset._Find_first();
  bitset.set(first, true);
  return first;
}

void BlockBitmap::set_block_free(size_t block_index)
{
  bitset.set(block_index, false);
}

Block::Block() {
  // Initialize the block buffer
  std::memset(buf, 0, BLOCK_SIZE);
}
void Block::read_block(char* buffer, size_t size, int offset)
{
  // Check if the offset and size are within the block bounds
  if (offset < 0 || offset >= BLOCK_SIZE || size <= 0 || offset + size > BLOCK_SIZE) {
    // Handle the out-of-bounds case
    return;
  }
  // Copy the requested data from the block buffer to the provided buffer
  std::memcpy(buffer, buf + offset, size);
}

void Block::write_block(char* buffer, size_t size, int offset)
{
  // Check if the offset and size are within the block bounds
  if (offset < 0 || offset >= BLOCK_SIZE || size <= 0 || offset + size > BLOCK_SIZE) {
    // Handle the out-of-bounds case
    return;
  }
  // Copy the provided data to the block buffer
  std::memcpy(buf + offset, buffer, size);
}

Inode::Inode(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode,FileSystem *fs)
{
  this->fs=fs;
  memset(&inode_metadata, 0, sizeof(inode_metadata));
  inode_metadata.st_ino = ino;
  inode_metadata.st_atime = time;
  inode_metadata.st_mtime = time;
  inode_metadata.st_ctime = time;
  inode_metadata.st_uid = uid;
  inode_metadata.st_gid = gid;
  inode_metadata.st_blksize = blksize;
  for (int i=0;i<DIRECT_POINTERS;i++)
    direct_pointers[i] = 0;
}

// Destructor for Inode
Inode::~Inode() {}

RegInode::RegInode(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode, FileSystem* fs)
: Inode(ino,time,uid,gid,blksize,mode,fs)
{
  inode_metadata.st_nlink = 1;
  inode_metadata.st_blocks = 0;
  inode_metadata.st_mode = S_IFREG | mode;  // mode_t : set read,write,execute permissions for each group
}

/**
     * Writes are atomic on the block level.
 */
off_t RegInode::write(char* buf, int size, int offset, FileSystem* fs)
{
  off_t bytes_written = 0;
  off_t block_idx;
  size_t block_offset;
  size_t remaining_size = size;

  while (remaining_size > 0)
  {
    // Calculate the block index and offset
    block_idx = (offset + bytes_written) / BLOCK_SIZE;
    block_offset = (offset + bytes_written) % BLOCK_SIZE;

    // Check if the block is already allocated, if not, allocate it
    if (direct_pointers[block_idx] == 0)
    {
      int32_t free_block_idx = fs->find_free_data_block();
      if (free_block_idx == -1)
      {
        // No free blocks available, return an error or the number of bytes written so far
        return (bytes_written > 0) ? bytes_written : -ENOSPC;
      }
      direct_pointers[block_idx] = free_block_idx;
    }

    // Calculate the number of bytes to write in the current block
    int bytes_to_write = std::min(BLOCK_SIZE - block_offset, remaining_size);

    // Write data to the block
    cown_ptr<Block> blk = fs->getDataBlock(block_idx);
    // Ask about this 'auto'
    when(blk) << [&] (acquired_cown<Block>  blk){
      blk->write_block( buf,bytes_to_write,offset);
    };

    bytes_written += bytes_to_write;
    remaining_size -= bytes_to_write;
  }

  // Update the file size and modify time
  inode_metadata.st_size = std::max(inode_metadata.st_size, offset + bytes_written);
  inode_metadata.st_mtime = time(nullptr);

  return bytes_written;
}

/**
 * This destructor should be called when Fuse kernel calls release (means no longer open by any file) and nlinks = 0
 */
RegInode::~RegInode() {
  // Release the inode
  fs->release_inode(inode_metadata.st_ino);
  // Release the data blocks
  for (uint32_t block_idx : direct_pointers) {
    if (block_idx != 0) {
      fs->release_data_block(block_idx);
    }
  }
}

DirInode::DirInode(fuse_ino_t ino, time_t time, uid_t uid, gid_t gid, blksize_t blksize, mode_t mode, FileSystem* fs) : Inode(ino,time,uid,gid,blksize,mode,fs)
{
  inode_metadata.st_nlink = 2;
  inode_metadata.st_blocks = 0;
  inode_metadata.st_mode = S_IFDIR | mode;
}


int find_block_with_free_space(){

  return 0;
}

bool DirInode::add_entry(const std::string& name, uint32_t ino){
  int idx = find_block_with_free_space();
  cown_ptr<Block> block = fs->getDataBlock(idx);
  when(block) << [&](acquired_cown<Block> blk){
    DirEntry* entries = reinterpret_cast<DirEntry*>(blk->buf);
    for (size_t i = 0; i < ENTRIES_PER_BLOCK; ++i) {
      if (entries[i].ino == 0) {
        // Found an empty slot. Add the new entry.
        entries[i].ino = ino;
        strncpy(entries[i].name, name.c_str(), sizeof(entries[i].name));
        entries[i].name[sizeof(entries[i].name) - 1] = '\0'; // Ensure null-termination

        return;
      }
    }

  };

}

FileSystem::FileSystem()
{
  for (int i = 0; i <NUM_DATABLOCKS; i++)
      data_blocks.emplace_back(make_cown<Block>());

}

int FileSystem::find_free_data_block()
{
  auto res = -1;
  when(block_bitmap) << [&](acquired_cown<BlockBitmap> bitmap){

    res = bitmap->find_free_block();
  };
  // wait somehow here ????
  return res;
}

void FileSystem::release_data_block(uint32_t block_idx)
{
  when(block_bitmap) << [&](acquired_cown<BlockBitmap> bitmap){
    bitmap->set_block_free(block_idx);
  };
}


int FileSystem::find_free_inode()
{
  auto res = -1;
  when(inode_bitmap) << [&](acquired_cown<InodeBitmap> bitmap){

    res = bitmap->find_free_inode();
  };
  // wait somehow here, spinlock will work ????
  return res;
}

void FileSystem::release_inode(uint32_t inode_idx)
{
  when(inode_bitmap) << [&](acquired_cown<InodeBitmap> bitmap){
    bitmap->release_inode(inode_idx);
  };
}




int main1(int argc, char *argv[]){
  std::cout << "Hello" << std::endl;
}







