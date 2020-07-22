//Camden Kronhaus

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "disk.h"

#define MAX_FILDES 32 //fds, integer between [0, 31]
#define MAX_FILES 64
#define RESERVED_BLOCKS 4096 //used to store meta information
#define MAX_BLOCKS 8192      //total block available, 4kb per block
//8,192 blocks available total
#define MAX_SIZE 16
//max file size is 16 megabytes, all 4,096 data blocks with 4kb each
#define MAX_F_NAME 15

#define EOFILE -1
#define FREE_SPACE -2
#define RESERVED_SPACE -3

#define super_head 0
#define dir_head 2
#define fat_head 4

#define free_block_start 25

//DATA STRUCTURES:

//super block
//stores information about location of other data structures

char *nullOverwrite; //deletes data in block


struct super_block
{
    int fat_idx;  // First block of the FAT
    int fat_len;  // Length of FAT in blocks
    int dir_idx;  // First block of directory
    int dir_len;  // Length of directory in blocks
    int data_idx; // First block of file-data
};

//directory entry (file metadata)
//holds names of files
//FAT also stores file size and head of list of corresponding data block
struct dir_entry
{
    int used;                  // Is this file-”slot” in use
    char name[MAX_F_NAME + 1]; // DOH!
    int size;                  // file size
    int head;                  // first data block of file
    int ref_cnt;
    int extra;
    // how many open file descriptors are there?
    // ref_cnt > 0 -> cannot delete file
};

//File Descriptor
struct file_descriptor
{
    int used;   // fd in use
    int file;   // the first block of the file
                // (f) to which fd refers too
    int offset; // position of fd within f
};
//file allocation table
//keeps track of empty blocks and mapping between files and data blocks

struct block
{
    char *bytes;
};

struct block *blocks;

struct super_block fs;
struct file_descriptor fildes[MAX_FILDES]; // 32
int FAT[DISK_BLOCKS];                      // Will be populated with the FAT data
struct dir_entry *DIR;                     // Will be populated with
                                           //the directory data

int make_fs(char *disk_name)
{

    nullOverwrite = (char *)calloc(4096, sizeof(char));
    int i;
    DIR = (struct dir_entry *)calloc(64, sizeof(struct dir_entry));
    blocks = (struct block *)calloc(8192, sizeof(struct block));
    for (i = 0; i < DISK_BLOCKS; i++)
    {
        blocks[i].bytes = (char *)calloc(4096, sizeof(char));
    }
    for (i = 0; i < MAX_FILDES; i++)
        fildes[i].used = 0;
    for (i = free_block_start; i < DISK_BLOCKS; i++)
    {
        FAT[i] = FREE_SPACE;
    }

    fs.fat_idx = fat_head;               // First block of the FAT
    fs.fat_len = sizeof(FAT) / 4096 + 1; // Length of FAT in blocks
    fs.dir_idx = dir_head;               // First block of directory
    fs.dir_len = sizeof(DIR) / 4096 + 1; // Length of directory in blocks
    fs.data_idx = free_block_start;      // First block of file-data

    if (make_disk(disk_name) < 0)
        return -1;
    if (open_disk(disk_name) < 0)
        return -1;
    if (block_write(super_head, (char *)&fs) < 0)
        return -1;
    if (block_write(dir_head, (char *)&DIR) < 0)
        return -1;
    if (block_write(fat_head, (char *)&FAT) < 0)
        return -1;
    // if (block_write(15, (char *)&fildes) < 0)
    //     return -1;
    if (close_disk(disk_name) < 0)
        return -1;

    return 0;
}

int mount_fs(char *disk_name)
{
    int i;
    if (open_disk(disk_name) < 0)
        return -1;
    for (i = free_block_start; i < DISK_BLOCKS; i++)
    {
        if (block_read(i, blocks[i].bytes) < 0)
            return -1;
    }
    if (block_read(super_head, (char *)&fs) < 0)
        return -1;
    if (block_read(dir_head, (char *)&DIR) < 0)
        return -1;
    if (block_read(fat_head, (char *)&FAT) < 0)
        return -1;
    // if (block_read(15, (char *)&fildes) < 0)
    //     return -1;

    return 0;
}

int umount_fs(char *disk_name)
{
    int i;
    for (i = free_block_start; i < DISK_BLOCKS; i++)
    {
        if (block_write(i, blocks[i].bytes) < 0)
            return -1;
    }
    if (block_write(super_head, (char *)&fs) < 0)
        return -1;
    // if (block_write(15, (char *)&fildes) < 0)
    //     return -1;
    if (block_write(dir_head, (char *)&DIR) < 0)
        return -1;
    if (block_write(fat_head, (char *)&FAT) < 0)
        return -1;
    if (close_disk(disk_name) < 0)
        return -1;
    return 0;
}

int fs_open(char *name)
{
    // if (block_read(dir_head, (char *)&DIR) < 0)
    //     return -1;
    int fd_index;
    int i;

    //find a useable fd
    bool found = 0;
    for (i = 0; i < MAX_FILDES; i++)
    {
        if (fildes[i].used == 0)
        {
            found = 1;
            fd_index = i;
            break;
        }
        //all fds have been used
    }
    if (!found)
        return -1;

    found = 0;
    //find directory entry with the same file name
    for (i = 0; i < MAX_FILES; i++)
    {
        if (strcmp(DIR[i].name, name) == 0)
        {
            //ref count of directory entry is increased by one fd
            DIR[i].ref_cnt++;
            found = 1;
            break;
        }
    }
    if (!found)
        return -1;

    if (DIR[i].used == 0)
    {
        return -1;
    }

    //change file descriptor to being used
    fildes[fd_index].used = 1;
    //head is the same place as fd file head
    fildes[fd_index].file = DIR[i].head;
    fildes[fd_index].offset = 0;

    // if (block_write(dir_head, (char *)&DIR) < 0)
    //     return -1;

    return fd_index;
}

int fs_close(int in_filde)
{
    //not a valid fd
    if (in_filde > 31 || in_filde < 0)
        return -1;
    //fd already closed
    if (fildes[in_filde].used == 0)
        return -1;

    int i;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (fildes[in_filde].file == DIR[i].head)
        {
            if (DIR[i].ref_cnt > 0)
            {
                (DIR[i].ref_cnt)--;
                break;
            }
            else
            {
                return -1;
            }
        }
    }

    fildes[in_filde].used = 0;
    fildes[in_filde].offset = 0;
    fildes[in_filde].file = 0;

    return 0;
}

int fs_create(char *name)
{
    if (sizeof(name) > 15 || sizeof(name) == 0) //file name too long
        return -1;
    //check if name is used already
    int i;
    for (i = 0; i < MAX_FILES; i++)
    {
        //name is already in use, can't create a repeat file
        if (strcmp(DIR[i].name, name) == 0)
            return -1;
    }
    //find useable directory
    int dir_index;
    bool found = 0;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (DIR[i].used == 0)
        {
            found = 1;
            dir_index = i;
            break;
        }
    }
    if (!found)
        return -1;

    found = 0;

    int fat_index;
    for (i = free_block_start; i < DISK_BLOCKS; i++)
    {
        if (FAT[i] == FREE_SPACE)
        {
            found = 1;
            fat_index = i;
            break;
        }
    }
    if (!found)
        return -1;

    FAT[fat_index] = EOFILE;
    DIR[dir_index].used = 1;
    strcpy(DIR[dir_index].name, name);
    DIR[dir_index].ref_cnt = 0;
    DIR[dir_index].size = 0;
    DIR[dir_index].head = fat_index;
    DIR[dir_index].extra = 0;

    return 0;
}

int fs_delete(char *name)
{
    int i;

    int dir_index;
    //look through directory for file name
    bool found;
    for (i = 0; i < MAX_FILES; i++)
    {
        //if filename is found check if it has open fds
        if (strcmp(DIR[i].name, name) == 0)
        {
            //if there is an open fd, return -1, else mark where file was found and continue delete
            if (DIR[i].ref_cnt > 0)
            {
                return -1;
            }
            else
            {
                found = 1;
                dir_index = i;
                break;
            }
        }
    }
    if (!found)
        return -1;
    found = 0;

    //delete all file information
    int fat_index = DIR[dir_index].head;
    int next_index = fat_index;
    //overwrite blocks corresponding to the file with an initialized character array nullOverwrite
    while (FAT[fat_index] != EOFILE)
    {
        memcpy(blocks[fat_index].bytes, nullOverwrite, 4096);
        next_index = FAT[fat_index];
        FAT[fat_index] = FREE_SPACE;
        fat_index = next_index;
    }
    memcpy(blocks[fat_index].bytes, nullOverwrite, 4096);
    FAT[fat_index] = FREE_SPACE;

    char newName[16];
    strcpy(DIR[dir_index].name, newName);
    DIR[dir_index].used = 0;
    DIR[dir_index].head = 0;
    DIR[dir_index].size = 0;

    return 0;
}

int fs_read(int in_filde, void *buf, size_t nbyte)
{
    int i;

    if (in_filde < 0 || in_filde > 31 || fildes[in_filde].used == 0)
        return -1;

    int file_pointer_increments = 0;                     //how many times we incremented the file_pointer to change offset later
    int file_pointer = (fildes[in_filde].offset) % 4096; //points to index within the block;
    int file_head = fildes[in_filde].file;               //used to keep track what block we're starting at
    //returns an integer corresponding to which block of the file we want (0th, 1st, 2nd, etc)
    int file_index = (fildes[in_filde].offset) / 4096; //index of file blocks (as if array)

    int block_index = file_head;
    int j = 0; //iterates until we're at the correct file_index
    while (j != file_index)
    {
        if (FAT[block_index] == EOFILE)
        {
            break;
        }
        else
        {
            block_index = FAT[block_index];
            j++;
        }
    }

    int count = 0; //used to count total number of bytes read
    i = 0;
    //offset
    while (count != nbyte) //stop after proper number of bytes read
    {
        //stop reading in bytes when at the end of a file
        if (file_pointer == 4096) //start reading from the next file block if not at EOFILE
        {
            if (FAT[block_index] == EOFILE)
            {
                // fildes[in_filde].offset += file_pointer_increments;
                // return count;
                break;
            }
            else
            {
                block_index = FAT[block_index];
                file_pointer = 0;
            }
        }
        if (blocks[block_index].bytes[file_pointer] == '\0')
        {
            // fildes[in_filde].offset += file_pointer_increments;
            // return count;
            break;
        }
        //read in a byte
        memcpy(buf + i, (blocks[block_index].bytes) + file_pointer, 1);
        i++;                       //read in to next index of i
        count++;                   //increase number of bytes read
        file_pointer++;            //read next byte in file block
        file_pointer_increments++; //we just incremented the file_pointer once, affecting the overall offset
    }

    fildes[in_filde].offset += (file_pointer_increments);

    return count;
}

//update dir.size
//update fd.offset
int fs_write(int in_filde, void *buf, size_t nbyte)
{
    int i;
    if (in_filde < 0 || in_filde > 31 || fildes[in_filde].used == 0)
        return -1;

    int dir_index;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (fildes[in_filde].file == DIR[i].head)
        {
            dir_index = i;
            break;
        }
    }

    // int file_pointer_increments = 0; //how many times we incremented the file_pointer to change offset later
    // int size_increments = 0;
    int file_pointer = (fildes[in_filde].offset) % 4096; //points to index within the block;
    int file_head = fildes[in_filde].file;               //used to keep track what block we're starting at
    //returns an integer corresponding to which block of the file we want (0th, 1st, 2nd, etc)
    int file_index = (fildes[in_filde].offset) / (4096); //index of file blocks (as if array)
    int block_index = file_head;
    int j = 0; //iterates until we're at the correct file_index
    int x = 0;
    bool found = 0;

    //if at the end of a file and need to add an extra block
    if (DIR[dir_index].extra == 1 && fildes[in_filde].offset == DIR[dir_index].size)
    {
        while (FAT[block_index] != EOF)
        {
            block_index = FAT[block_index];
        }
        found = 0;
        for (x = free_block_start; x < DISK_BLOCKS; x++)
        {
            if (FAT[x] == FREE_SPACE)
            {
                found = 1;
                FAT[block_index] = x;
                FAT[x] = EOFILE;
                block_index = x;
                file_pointer = 0;
                DIR[dir_index].extra = 0;
                break;
            }
        }
        if (!found) //out of disk space
        {
            return 0;
            printf("out of disk space");
        }
    }
    block_index = file_head;
    while (j != file_index)
    {
        if (FAT[block_index] == EOFILE)
        {
            printf("ERROR");
            return 0;
        }
        else
        {
            block_index = FAT[block_index];
            j++;
        }
    }

    int count = 0; //used to count total number of bytes written
    i = 0;
    int totalSize = DIR[dir_index].size;
    int totalOffset = fildes[in_filde].offset;
    //offset
    while (count != nbyte) //stop after proper number of bytes read
    {
        if (totalSize == 16777216)
        {
            break;
        }
        if (file_pointer == 4096) //start reading from the next file block if not at EOFILE
        {
            if (FAT[block_index] == EOFILE)
            {
                found = 0;
                for (x = free_block_start; x < DISK_BLOCKS; x++)
                {
                    if (FAT[x] == FREE_SPACE)
                    {
                        found = 1;
                        FAT[block_index] = x;
                        FAT[x] = EOFILE;
                        block_index = x;
                        file_pointer = 0;
                        break;
                    }
                }
                if (!found) //out of disk space
                {
                    // DIR[dir_index].size += size_increments;
                    return count;
                }
            }
            else
            {
                block_index = FAT[block_index];
                file_pointer = 0;
            }
        }
        //read in a byte
        memcpy((blocks[block_index].bytes) + file_pointer, buf + i, 1);
        i++;            //read in to next index of i
        count++;        //increase number of bytes written
        file_pointer++; //read next byte in file block
        //file_pointer_increments++; //we just incremented the file_pointer once, affecting the overall offset
        if (FAT[block_index] == EOFILE)
            totalSize++;
        totalOffset++;
    }

    fildes[in_filde].offset = totalOffset;
    DIR[dir_index].size = totalSize;
    if (file_pointer == 4096 && FAT[block_index] == EOFILE)
    {
        DIR[dir_index].extra = 1;
    }
    //assign another block to the file, but don't write to it or increase size yet

    return count;
}
//lwoodman@redhat.com
int fs_get_filesize(int in_filde)
{
    if (in_filde < 0 || in_filde > 31 || fildes[in_filde].used == 0)
        return -1;
    int i;
    int dir_index;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (DIR[i].head == fildes[in_filde].file)
        {
            dir_index = i;
            break;
        }
    }
    return DIR[dir_index].size;
}
int fs_listfiles(char ***files)
{
    int i;
    char **all_files = calloc(64, sizeof(char *));
    for (i = 0; i < 64; i++)
    {
        all_files[i] = calloc(16, sizeof(char));
    }

    int j = 0;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (DIR[i].used == 1)
        {
            strcpy(all_files[j], DIR[i].name);
            j++;
        }
    }
    char *nullChar = "\0";
    strcpy(all_files[j], nullChar);

    *files = all_files;
    return 0;
}

int fs_truncate(int in_filde, off_t length)
{
    int i;
    if (in_filde < 0 || in_filde > 31 || fildes[in_filde].used == 0)
        return -1;

    int dir_index;
    for (i = 0; i < MAX_FILES; i++)
    {
        if (fildes[in_filde].file == DIR[i].head)
        {
            dir_index = i;
            break;
        }
    }

    if (DIR[dir_index].size == length)
    {
        return 0;
    }

    if (DIR[dir_index].size < length)
    {
        return -1;
    }

    // int file_pointer_increments = 0; //how many times we incremented the file_pointer to change offset later
    // int size_increments = 0;
    int file_pointer = (length) % 4096;    //points to index within the block;
    int file_head = fildes[in_filde].file; //used to keep track what block we're starting at
    //returns an integer corresponding to which block of the file we want (0th, 1st, 2nd, etc)
    int file_index = length / (4096); //index of file blocks (as if array)
    int block_index = file_head;
    int j = 0; //iterates until we're at the correct file_index
    while (j != file_index)
    {
        if (FAT[block_index] == EOFILE)
        {
            printf("ERROR");
            return 0;
        }
        else
        {
            block_index = FAT[block_index];
            j++;
        }
    }

    int initialBlock = block_index;

    char *nullChar = "\0";
    //offset
    int totalSize = DIR[dir_index].size;
    int x;
    while (totalSize != length) //stop after proper number of bytes read
    {
        if (file_pointer == 4096) //start reading from the next file block if not at EOFILE
        {
            if (FAT[block_index] == EOF)
            {
                FAT[block_index] = FREE_SPACE;
                break;
            }
            else
            {
                x = FAT[block_index];
                FAT[block_index] = FREE_SPACE;
                block_index = x;
                file_pointer = 0;
            }
        }
        if (blocks[block_index].bytes[file_pointer] == '\0')
        {
            // fildes[in_filde].offset += file_pointer_increments;
            // return count;
            break;
        }

        //read in a byte
        memcpy((blocks[block_index].bytes) + file_pointer, nullChar, 1);
        totalSize--;
        file_pointer++; //read next byte in file block
        //file_pointer_increments++; //we just incremented the file_pointer once, affecting the overall offset
    }

    FAT[initialBlock] = EOFILE;

    DIR[dir_index].size = totalSize;
    fildes[in_filde].offset = totalSize;

    return 0;
}

int fs_lseek(int in_filde, off_t offset)
{
    if (offset < 0 || in_filde < 0 || in_filde > 31 || fildes[in_filde].used == 0)
        return -1;

    bool found = 0;
    int i;
    //find directory entry with the same file name
    for (i = 0; i < MAX_FILES; i++)
    {
        if (DIR[i].head == fildes[in_filde].file)
        {
            found = 1;
            break;
        }
    }
    if (!found)
        return -1;
    if (offset > DIR[i].size)
        return -1;

    fildes[in_filde].offset = offset;
    return 0;
}