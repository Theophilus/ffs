#define FUSE_USE_VERSION 30
#include <fuse_lowlevel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>

FILE *log_fp;
char *log_file_path;
char *stats_file_path="stats.txt";
int fs_size;
int fs_read=0;
int fs_write=0;
int gc_called=0;
int gc_write=0;
time_t mount;
time_t unmount;
int block_size = 4096;
int num_blocks;
int free_space;
int offset =0;

typedef struct meta_data{
  char *file_name;
  char *file_type;
  char *parent;
  char **children;
  char *created;
  char *updated;
  int data_size;
  int meta_size;
  int data_offset;
}Meta_data;

Meta_data **dictionary;
int curr_block=3;
static const char *hello_str = "Hello World!\n";
static const char *hello_name = "hello";

static int lfs_stat(fuse_ino_t ino, struct stat *stbuf){

  stbuf->st_ino = ino;
  switch (ino) {
  case 1:
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 2;
    break;
  case 2:
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    stbuf->st_size = strlen(hello_str);
    break;
  default:
    return -1;
  }
  return 0;
}

static void lfs_ll_getattr(fuse_req_t req, fuse_ino_t ino,
                             struct fuse_file_info *fi){
  struct stat stbuf;
  (void) fi;
  memset(&stbuf, 0, sizeof(stbuf));
  if (lfs_stat(ino, &stbuf) == -1)
    fuse_reply_err(req, ENOENT);
  else
    fuse_reply_attr(req, &stbuf, 1.0);
}

static void lfs_ll_lookup(fuse_req_t req, fuse_ino_t parent, const char *name){
  struct fuse_entry_param e;
  if (parent != 1 || strcmp(name, hello_name) != 0)
    fuse_reply_err(req, ENOENT);
  else {
    memset(&e, 0, sizeof(e));
    e.ino = 2;
    e.attr_timeout = 1.0;
    e.entry_timeout = 1.0;
    lfs_stat(e.ino, &e.attr);
    fuse_reply_entry(req, &e);
  }
}

struct dirbuf {
  char *p;
  size_t size;
};

static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name,
                       fuse_ino_t ino){
  struct stat stbuf;
  size_t oldsize = b->size;
  b->size += fuse_add_direntry(req, NULL, 0, name, NULL, 0);
  b->p = (char *) realloc(b->p, b->size);
  memset(&stbuf, 0, sizeof(stbuf));
  stbuf.st_ino = ino;
  fuse_add_direntry(req, b->p + oldsize, b->size - oldsize, name, &stbuf,
		    b->size);
}

#define min(x, y) ((x) < (y) ? (x) : (y))
static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize,
                             off_t off, size_t maxsize){
  if (off < bufsize)
    return fuse_reply_buf(req, buf + off,
			  min(bufsize - off, maxsize));
  else
    return fuse_reply_buf(req, NULL, 0);
}

static void lfs_ll_readdir(fuse_req_t req, fuse_ino_t ino, size_t size,
                             off_t off, struct fuse_file_info *fi){
  (void) fi;
  if (ino != 1)
    fuse_reply_err(req, ENOTDIR);
  else {
    struct dirbuf b;
    memset(&b, 0, sizeof(b));
    dirbuf_add(req, &b, ".", 1);
    dirbuf_add(req, &b, "..", 1);
    dirbuf_add(req, &b, hello_name, 2);
    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);
  }
}

static void lfs_ll_open(fuse_req_t req, fuse_ino_t ino,
                          struct fuse_file_info *fi){
  if (ino != 2)
    fuse_reply_err(req, EISDIR);
  else if ((fi->flags & 3) != O_RDONLY)
    fuse_reply_err(req, EACCES);
  else
    fuse_reply_open(req, fi);
}

static void lfs_ll_read(fuse_req_t req, fuse_ino_t ino, size_t size,
                          off_t off, struct fuse_file_info *fi){
  (void) fi;
  assert(ino == 2);
  reply_buf_limited(req, hello_str, strlen(hello_str), off, size);
}

static void lfs_ll_write(fuse_req_t req, fuse_ino_t ino, const char *buf, 
			 size_t size, off_t off, struct fuse_file_info *fi){
  Meta_data new_blk;
  new_blk.file_name=fi->fh;
  printf("%s",fi->fh);
  dictionary[curr_block+1]= malloc(sizeof(Meta_data)); 

  if(ino != 1)
    fuse_reply_err(req,ENOENT);
  else
    //pwrite(log_fp, buf, size, offset);
    fuse_reply_write(req,fi);


}

static struct fuse_lowlevel_ops lfs_ll_oper = {
  .lookup         = lfs_ll_lookup,
  .getattr        = lfs_ll_getattr,
  .readdir        = lfs_ll_readdir,
  .open           = lfs_ll_open,
  .read           = lfs_ll_read,
  .write          = lfs_ll_write,
};

int write_stats(){
  FILE *stat_fp;
  int n;
  char buff[300];
  struct tm *tsm,*tsu;
  char buf1[80];
  char buf2[80];
  
  tsm = localtime(&mount);
  strftime(buf1, sizeof(buf1), "%a %Y-%m-%d %H:%M:%S %Z", tsm);
  tsu = localtime(&unmount);
  strftime(buf2, sizeof(buf2), "%a %Y-%m-%d %H:%M:%S %Z", tsu);
  n=sprintf(buff,"Mount time:%s\nfs_size: %dMB\nfs_read:%dMB\nfs_write:%dMB\ngc_called:%d\ngc_write:%dMB\nUnmount\
 time:%s\n",buf1,(fs_size/1000000),fs_read,fs_write,
            gc_called,gc_write,buf2);

  stat_fp = fopen (stats_file_path,"a+");
  if (stat_fp == NULL) {
    printf ("error opening stats\n");
    return 1;
  }
  fseek(stat_fp,0l,SEEK_END);
  fwrite(buff,sizeof(char),n,stat_fp);
  fclose(stat_fp);
  return 0;
}


int main(int argc, char *argv[]){
  // Initialize an empty argument list                                                  
  struct fuse_args mountpt = FUSE_ARGS_INIT(0, NULL);
  // add program and mountpoint                                   
  fuse_opt_add_arg(&mountpt, argv[0]);
  fuse_opt_add_arg(&mountpt, argv[1]);
  fuse_opt_add_arg(&mountpt, argv[2]);// for debug                            
  fuse_opt_add_arg(&mountpt, argv[3]);// for debug                   

  log_file_path= argv[4];
  fs_size=atoi(argv[5])*1000000;
  num_blocks=fs_size/block_size;
  free_space=block_size*4;
  dictionary= malloc(num_blocks*sizeof(int));
  log_fp = fopen (log_file_path,"a+");
  if (log_fp == NULL) {
    printf ("Data file could not be opened\n");
    return 1;
  }
  mount=time(NULL);

  struct fuse_args args = FUSE_ARGS_INIT(mountpt.argc, mountpt.argv);
  struct fuse_chan *ch;
  char *mountpoint;
  int err = -1;
  if (fuse_parse_cmdline(&args, &mountpoint, NULL, NULL) != -1 &&
      (ch = fuse_mount(mountpoint, &args)) != NULL) {
    struct fuse_session *se;
    se = fuse_lowlevel_new(&args, &lfs_ll_oper,
			   sizeof(lfs_ll_oper), NULL);
    if (se != NULL) {
      if (fuse_set_signal_handlers(se) != -1) {
	fuse_session_add_chan(se, ch);
	/* Block until ctrl+c or fusermount -u */
	err = fuse_session_loop(se);
	fuse_remove_signal_handlers(se);
	fuse_session_remove_chan(ch);
      }
      fuse_session_destroy(se);
      unmount=time(NULL);
      write_stats();
    }
    fuse_unmount(mountpoint, ch);
  }
  fuse_opt_free_args(&args);
  free(dictionary);
  fclose(log_fp);
  return err ? 1 : 0;

}
