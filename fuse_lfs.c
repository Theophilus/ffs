#define FUSE_USE_VERSION 30

#include "stdlib.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <dirent.h>

static const char *log_file_path = NULL;
int log_file_size;

static int lfs_getattr(const char *path, struct stat *stbuf){

  int res;

  res = lstat(path, stbuf);
  if (res == -1)
    return -errno;

  return 0;
}

static int lfs_access(const char *path, int mask){
  int res;

  res = access(path, mask);
  if (res == -1)
    return -errno;

  return 0;
}

static int lfs_readlink(const char *path, char *buf, size_t size){
  int res;

  res = readlink(path, buf, size - 1);
  if (res == -1)
    return -errno;

  buf[res] = '\0';
  return 0;
}

static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi){
  DIR *dp;
  struct dirent *de;

  (void) offset;
  (void) fi;

  dp = opendir(path);
  if (dp == NULL)
    return -errno;

  while ((de = readdir(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = de->d_ino;
    st.st_mode = de->d_type << 12;
    if (filler(buf, de->d_name, &st, 0))
      break;
  }

  closedir(dp);
  return 0;
}

static int lfs_open(const char *path, struct fuse_file_info *fi){
  int res;

  res = open(path, fi->flags);
  if (res == -1)
    return -errno;

  close(res);
  return 0;

}

static int lfs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi){
  int fd;
  int res;

  (void) fi;
  fd = open(path, O_RDONLY);
  if (fd == -1)
    return -errno;

  res = pread(fd, buf, size, offset);
  if (res == -1)
    res = -errno;

  close(fd);
  return res;
}

static int lfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi){
  int fd;
  int res;
  (void) fi;
  fd = open(path, O_WRONLY);
  if (fd == -1)
    return -errno;
  res = pwrite(fd, buf, size, offset);
  if (res == -1)
    res = -errno;
  close(fd);
  return res;
}

static int lfs_mkdir(const char *path, mode_t mode){
  int res;
  res = mkdir(path, mode);
  if (res == -1)
    return -errno;
  return 0;
}

static int lfs_rmdir(const char *path){
  int res;
  res = rmdir(path);
  if (res == -1)
    return -errno;
  return 0;
}

static int lfs_rename(const char *from, const char *to){
  int res;
  res = rename(from, to);
  if (res == -1)
    return -errno;
  return 0;
}

static int lfs_statfs(const char *path, struct statvfs *stbuf){
  int res;
  res = statvfs(path, stbuf);
  if (res == -1)
    return -errno;
  return 0;
}

static struct fuse_operations lfs_oper = {
  .getattr        = lfs_getattr,
  .readdir        = lfs_readdir,
  .statfs         = lfs_statfs,
  .readlink       = lfs_readlink,
  .access         = lfs_access,
  // file operations
  .open           = lfs_open,
  .read           = lfs_read,
  .write          = lfs_write,
  .rename         = lfs_rename,
  //dir operations
  .rmdir          = lfs_rmdir,
  .mkdir          = lfs_mkdir,
};

int main(int argc, char *argv[]){

  // Initialize an empty argument list
  struct fuse_args mountpt = FUSE_ARGS_INIT(0, NULL);
  // add program and mountpoint
  fuse_opt_add_arg(&mountpt, argv[0]);
  fuse_opt_add_arg(&mountpt, argv[1]);
  //fuse_opt_add_arg(&mountpt, argv[4]);// for permission
  log_file_path= argv[2];
  log_file_size= argv[3];
     
  return fuse_main(mountpt.argc, mountpt.argv, &lfs_oper, NULL);
}