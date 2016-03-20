/*
 * Fast file copying
 * Based on memory-mapped files
 * Victor Manuel Fernandez-Castro
 * 19 March 2016
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/limits.h>

// Copies content between files. Return -1 on error.
static int copy(int fd_orig, int fd_dest, size_t length);

// Copies directories recorsiverly
static int copy_dir(int dd_orig, int dd_dest);

// Information for debugging (activate with -DDEBUG)
static void debug(const char *format, ...);

// Main ------------------------------------------------------------------------

int main(int argc, char **argv) {
    int fd_orig;
    int fd_dest;
    int recorsive = 0;
    const char *path_orig;
    const char *path_dest;
    struct stat stat_orig;
    struct stat stat_dest;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s [-R] <origin> <destination>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc >= 4 && !strcmp(argv[1], "-R")) {
        recorsive = 1;
        path_orig = argv[2];
        path_dest = argv[3];
    } else {
        path_orig = argv[1];
        path_dest = argv[2];
    }

    fd_orig = open(path_orig, O_RDONLY);

    if (fd_orig < 0) {
        fprintf(stderr, "Can't open file %s: %s\n", path_orig, strerror(errno));
        return EXIT_FAILURE;
    }

    if (fstat(fd_orig, &stat_orig) < 0) {
        fprintf(stderr, "Can't get information about %s: %s\n", path_orig, strerror(errno));
        return EXIT_FAILURE;
    }

    switch (stat_orig.st_mode & S_IFMT) {
    case S_IFDIR:
        if (!recorsive) {
            fprintf(stderr, "%s is a directory. Use -R\n", path_orig);
            return EXIT_FAILURE;
        }

        fd_dest = open(path_dest, O_RDONLY);

        // If target directory doesn't exist, create it

        if (fd_dest < 0) {
            if (mkdir(path_dest, stat_orig.st_mode) < 0) {
                fprintf(stderr, "Can't create directory %s: %s\n", path_dest, strerror(errno));
                return EXIT_FAILURE;
            }

            fd_dest = open(path_dest, O_RDONLY);

            if (fd_dest < 0) {
                fprintf(stderr, "Can't open directory %s: %s\n", path_dest, strerror(errno));
                return EXIT_FAILURE;
            }
        } else {

            // If target isn't a directory, exit

            if (fstat(fd_dest, &stat_dest) < 0) {
                fprintf(stderr, "Can't get information about %s: %s\n", path_dest, strerror(errno));
                return EXIT_FAILURE;
            }

            if (!S_ISDIR(stat_dest.st_mode)) {
                fprintf(stderr, "%s is not a directory\n",path_dest);
                return EXIT_FAILURE;
            }
        }

        debug("copy_dir %s -> %s", path_orig, path_dest);

        if (copy_dir(fd_orig, fd_dest) < 0) {
            fprintf(stderr, "Can't copy directory %s: %s\n", path_orig, strerror(errno));
            return EXIT_FAILURE;
        }

        break;

    case S_IFREG:

        // If destination exists and it isn't a regular file, exit

        if (stat(path_dest, &stat_dest) < 0) {
            fd_dest = open(path_dest, O_RDWR | O_CREAT | O_TRUNC, stat_orig.st_mode);
            if (fd_dest < 0) {
                fprintf(stderr, "Can't create %s: %s\n", path_dest, strerror(errno));
                return EXIT_FAILURE;
            }
        } else {
            char path_buf[PATH_MAX];
            strncpy(path_buf, path_dest, PATH_MAX);

            switch (stat_dest.st_mode & S_IFMT) {
            case S_IFDIR:
                strncat(path_buf, "/", PATH_MAX - strlen(path_buf));
                strncat(path_buf, basename(strdup(path_orig)), PATH_MAX - strlen(path_buf));

            case S_IFREG:
                debug("Creating %s", path_buf);
                fd_dest = open(path_buf, O_RDWR | O_CREAT | O_TRUNC, stat_orig.st_mode);

                if (fd_dest < 0) {
                    fprintf(stderr, "Can't create %s: %s\n", path_buf, strerror(errno));
                    return EXIT_FAILURE;
                }

                break;

            default:
                fprintf(stderr, "%s is not a regular file\n",path_dest);
                return EXIT_FAILURE;
            }
        }

        debug("copy_file %s -> %s (%zu)", path_orig, path_dest, (size_t)stat_orig.st_size);

        if (copy(fd_orig, fd_dest, (size_t)stat_orig.st_size) < 0) {
            fprintf(stderr, "Can't copy %s: %s\n", path_orig, strerror(errno));
            return EXIT_FAILURE;
        }

        break;

    default:
        fprintf(stderr, "%s isn't a file neither a directory\n", path_orig);
        return EXIT_FAILURE;
    }

    // File descriptors will be closed

    return EXIT_SUCCESS;
}

// Copies content between files. Return -1 on error. ---------------------------

static int copy(int fd_orig, int fd_dest, size_t length) {
    void *mp_orig;
    void *mp_dest;
    
    if (length < 1) {
        debug("Skipping copy: length < 1");
        return 0;
    }

    if (lseek(fd_dest, length - 1, SEEK_SET) < 0) {
        debug("Error at lseek()");
        return -1;
    }

    if (write(fd_dest, "", 1) < 0) {
        debug("Error at write()");
        return -1;
    }

    mp_orig = mmap(NULL, length, PROT_READ, MAP_SHARED, fd_orig, 0);

    if (mp_orig == MAP_FAILED) {
        debug("Error at mmap(fd_orig)");
        return -1;
    }

    mp_dest = mmap(NULL, length, PROT_WRITE, MAP_SHARED, fd_dest, 0);

    if (mp_dest == MAP_FAILED) {
        debug("Error at mmap(fd_dest)");
        return -1;
    }

    memcpy(mp_dest, mp_orig, length);
    munmap(mp_orig, length);
    munmap(mp_dest, length);

    return 0;
}

// Copies directories recorsiverly ---------------------------------------------

static int copy_dir(int dd_orig, int dd_dest) {
    int fd_orig;
    int fd_dest;
    int retval = 0;
    struct dirent *entry;
    struct stat stat_orig;
    struct stat stat_dest;
    DIR *dir = fdopendir(dd_orig);

    if (!dir)
        return -1;

    while ((entry = readdir(dir))) {
        if (!(strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")))
            continue;
        
        fd_dest = -1;
        fd_orig = openat(dd_orig, entry->d_name, O_RDONLY);

        if (fd_orig < 0)
            return -1;

        if (fstat(fd_orig, &stat_orig) < 0)
            return -1;

        switch (stat_orig.st_mode & S_IFMT) {
        case S_IFDIR:
            fd_dest = openat(dd_dest, entry->d_name, O_RDONLY);

            // If target directory doesn't exist, create it

            if (fd_dest < 0) {
                if (mkdirat(dd_dest, entry->d_name, stat_orig.st_mode) < 0) {
                    fprintf(stderr, "Omitting %s\n", entry->d_name);
                    break;
                }

                fd_dest = openat(dd_dest, entry->d_name, O_RDONLY);

                if (fd_dest < 0) {
                    fprintf(stderr, "Omitting %s\n", entry->d_name);
                    break;
                }
            } else {

                // If target isn't a directory, omit it

                if (fstat(fd_dest, &stat_dest) < 0)
                    return -1;

                if (!S_ISDIR(stat_dest.st_mode)) {
                    fprintf(stderr, "Omitting %s\n", entry->d_name);
                    break;
                }
            }

            debug("copy_dir %s", entry->d_name);
            retval = copy_dir(fd_orig, fd_dest);
            break;

        case S_IFREG:

            // If destination exists and it isn't a regular file, omit

            fd_dest = openat(dd_dest, entry->d_name, O_RDWR | O_CREAT | O_TRUNC, stat_orig.st_mode);
            if (fd_dest < 0) {
                fprintf(stderr, "Omitting %s\n", entry->d_name);
                break;
            }

            if (fstat(fd_dest, &stat_dest) < 0)
                return -1;

            if (!S_ISREG(stat_dest.st_mode)) {
                fprintf(stderr, "Omitting %s\n", entry->d_name);
                break;
            }

            debug("copy_file %s (%zu)", entry->d_name, (size_t)stat_orig.st_size);
            retval = copy(fd_orig, fd_dest, (size_t)stat_orig.st_size);
            break;

        default:
            fprintf(stderr, "Omitting %s\n", entry->d_name);
        }

        close(fd_orig);
        close(fd_dest);

        if (retval < 0)
            return -1;
    }

    return 0;
}

// Information for debugging (activate with -DDEBUG) ---------------------------

#ifdef DEBUG
static void debug(const char *format, ...) {
    va_list arg;
    va_start(arg, format);
    printf("DEBUG: ");
    vprintf(format, arg);
    printf("\n");
    va_end(arg);
}
#else
static void debug(__attribute__((unused)) const char *format, ...) {
}
#endif
