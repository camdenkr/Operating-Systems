#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main()
{
    // int offset = 0;
    // int file_pointer = offset;

    // int file_index = offset/3;

    // file_pointer = offset%3;
    // printf("%d\n", file_pointer);
    // printf("%d\n", file_index);

    struct dir_entry
    {
        char name[16];
    };

    struct dir_entry *DIR; // Will be populated with

    DIR = (struct dir_entry *)calloc(64, sizeof(struct dir_entry));

    char *disk_name = "DISK A";
    //char *file_nameA = "FILE A";
    char *file_nameB = "FILE B";

    make_fs(disk_name);
    mount_fs(disk_name);

    int a = umount_fs(disk_name);
    
    int i;
    fs_create(file_nameB);
    int fd = fs_open(file_nameB);

    void *write_buf;
    char *character = "X";
    write_buf = (char *)calloc(16777216, sizeof(char));
    for (i = 0; i < 16777216; i++)
    {
        memcpy(write_buf + i, character, 1);
    }
    fs_write(fd, write_buf, 16777216); //write 15mb
    fs_lseek(fd, 0);                 //go to teh 15mb position
    int length = fs_get_filesize(fd);

    printf("%d\n", length);
    void *read_buf;
    read_buf = calloc(16777217, sizeof(char));

    fs_read(fd, read_buf, 16777216);

    char *assign_buf;

    assign_buf = (char *)read_buf;

    for (i = 0; i < 2; i++)
    {
        printf("Byte %d: %c\n", i, assign_buf[i]);
    }

    return 0;
}