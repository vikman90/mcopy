/*
 * Fast file copying
 * Based on memory-mapped files
 * Victor Manuel Fernandez-Castro
 * 19 March 2016
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#define F_MODE 0664

/* Copies content between files. Return -1 on error. */
static int copy(int fd_orig, int fd_dest, size_t length);

int main(int argc, char **argv) {
    int fd_orig;
    int fd_dest;
    struct stat stat_orig;
    
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <origin> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    fd_orig = open(argv[1], O_RDONLY);
    if (fd_orig < 0) {
        fprintf(stderr, "Can't open file %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }

    /* Target file must be opened in mode read/write */
    fd_dest = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, F_MODE);
    if (fd_dest < 0) {
        fprintf(stderr, "Can't create file %s: %s\n", argv[2], strerror(errno));
        return EXIT_FAILURE;
    }
    
    if (fstat(fd_orig, &stat_orig) < 0) {
        fprintf(stderr, "Can't get information about %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    }
    
    if (copy(fd_orig, fd_dest, (size_t)stat_orig.st_size) < 0) {
        fprintf(stderr, "Can't open file %s: %s\n", argv[1], strerror(errno));
        return EXIT_FAILURE;
    } else
        return EXIT_SUCCESS;
    
    /* File descriptors will be closed */
}

/* Copies content between files. Return -1 on error. */
static int copy(int fd_orig, int fd_dest, size_t length) {
    void *mp_orig;
    void *mp_dest;
    
    if (lseek(fd_dest, length - 1, SEEK_SET) < 0)
        return -1;
    
    if (write(fd_dest, "", 1) < 0)
        return -1;
    
    mp_orig = mmap(NULL, length, PROT_READ, MAP_SHARED, fd_orig, 0);
    
    if (mp_orig == MAP_FAILED)
        return -1;
    
    mp_dest = mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd_dest, 0);
    
    if (mp_dest == MAP_FAILED)
        return -1;

    memcpy(mp_dest, mp_orig, length);
    munmap(mp_orig, length);
    munmap(mp_dest, length);
    
    return 0;
}
