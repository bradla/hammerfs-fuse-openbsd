#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>    /* struct stat */
#include <dirent.h>      /* struct dirent */
#include "hammerread.c"
/* #include "hammerread.h"
*/

//ino_t hlookup(struct hfs *hfs, const char *path);

struct hfs hfs;

static int hammer_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    ino_t ino = hlookup(&hfs, path);
    if(ino == (ino_t)-1) {
        return -ENOENT;
    }

    if(hstat(&hfs, ino, stbuf)) {
        return -ENOENT;
    }

    return 0;
}

static int hammer_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    ino_t ino = hlookup(&hfs, path);
    if(ino == (ino_t)-1) {
        return -ENOENT;
    }

    struct stat st;
    if(hstat(&hfs, ino, &st)) {
        return -ENOENT;
    }

    if(!S_ISDIR(st.st_mode)) {
        return -ENOENT;
    }

    if(offset == 0) {
      filler(buf, ".", NULL, 0);
      filler(buf, "..", NULL, 0);
    }

    struct dirent de;
    while(hreaddir(&hfs, ino, &offset, &de) == 0) {
      if(filler(buf, de.d_name, NULL, 0)) {
        return 0;
      }
    }

    return 0;
}

static int hammer_open(const char *path, struct fuse_file_info *fi)
{
    ino_t ino = hlookup(&hfs, path);
    if(ino == (ino_t)-1) {
        return -ENOENT;
    }

    struct stat st;
    if(hstat(&hfs, ino, &st)) {
        return -ENOENT;
    }

    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;

    return 0;
}

static int hammer_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
    (void) fi;

    ino_t ino = hlookup(&hfs, path);
    if(ino == (ino_t)-1) {
        return -ENOENT;
    }

    struct stat st;
    if(hstat(&hfs, ino, &st)) {
        return -ENOENT;
    }

    if(S_ISDIR(st.st_mode)) {
        return -EISDIR;
    }

    if (offset < st.st_size) {
        if (offset + size > st.st_size)
            size = st.st_size - offset;
        hreadf(&hfs, ino, offset, size, buf);
    } else
        size = 0;

    return size;
}

static int hammer_readlink(const char *path, char *buf, size_t size)
{
    int res;

    ino_t ino = hlookup(&hfs, path);
    if(ino == (ino_t)-1) {
        return -ENOENT;
    }

    res = hreadlink(&hfs, ino, buf, size);

    if(res < 0)
	return -EINVAL;

    return res;
}

static void usage(char* argv[])
{
    printf("HAMMER filesystem for FUSE (readonly)\n");
    printf("Usage: %s <dev> <mount_point> [<FUSE library options>]\n", argv[0]);
}

static struct fuse_operations hammer_oper = {
    .getattr	= hammer_getattr,
    .readdir	= hammer_readdir,
    .open	= hammer_open,
    .read	= hammer_read,
    .readlink = hammer_readlink,
};

/*
struct fuse_operations bb_oper = {
  .getattr = bb_getattr,
  .readlink = bb_readlink,
  .mknod = bb_mknod,
  .mkdir = bb_mkdir,
  .unlink = bb_unlink,
  .rmdir = bb_rmdir,
  .symlink = bb_symlink,
  .rename = bb_rename,
  .link = bb_link,
  .chmod = bb_chmod,
  .chown = bb_chown,
  .truncate = bb_truncate,
  .utime = bb_utime,
  .open = bb_open,
  .read = bb_read,
  .write = bb_write,
  .statfs = bb_statfs,
  .flush = bb_flush,
  .release = bb_release,
  .fsync = bb_fsync,
  .setxattr = bb_setxattr,
  .getxattr = bb_getxattr,
  .listxattr = bb_listxattr,
  .removexattr = bb_removexattr,
  .opendir = bb_opendir,
  .readdir = bb_readdir,
  .releasedir = bb_releasedir,
  .fsyncdir = bb_fsyncdir,
  .init = bb_init,
  .destroy = bb_destroy,
  .access = bb_access,
  .create = bb_create,
  .ftruncate = bb_ftruncate,
  .fgetattr = bb_fgetattr
}; 
*/

int main(int argc, char *argv[])
{
    int ret;

    if(argc < 3) {
        usage(argv);
        exit(EINVAL);
    }

    hfs.fd = open(argv[1], O_RDONLY);
    if (hfs.fd == -1) {
        fprintf(stderr, "unable to open %s", argv[1]);
        exit(EINVAL);
    }

    if (hinit(&hfs) == -1) {
        fprintf(stderr, "invalid hammerfs");
        exit(EINVAL);
    }

    argc--;
    argv++;

    ret = fuse_main(argc, argv, &hammer_oper, NULL);

    hclose(&hfs);

    return ret;
}
