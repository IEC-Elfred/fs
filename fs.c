#include "fs.h"
#include "syscalls.h"
#include <sys/stat.h>
#include "diskio.h"
#include "spblk.h"
#include "inode.h"
#include "datablk.h"
#include "dir.h"


int log_count = 0;
pthread_mutex_t sp_lock;

void log_print(const char* text)
{
    fprintf(LOG_FILE, "%d: %s", log_count, text);
    log_count++;
}

int load_disk(char* disk_path)
{
   // disk = malloc(disk_size);
    if ((disk_fd = open(disk_path, O_RDWR)) == -1) {
        fprintf(stderr, "Disk loaded error!\n");
        return -1;
    }

    fprintf(stderr, "Disk loaded!\n");
    return 0;
}

void dump_disk(char* path)
{
   // FILE* fp = fopen(path,"wb");
   // fwrite(disk, sizeof(BYTE), DISK_SIZE, fp);
   // fclose(fp);

}

void fs_init()
{
   // super_init();
   // super_write();
    super_read();
    pthread_mutex_init(&sp_lock, NULL);
   /* sp_blk.fs_size = 1;
    sp_blk.free_blks_num = 1;
    sp_blk.inode_lst_sz = 1;
    sp_blk.free_inodes_num = 1;

    int i;

    for (i = 0; i < SP_BLK_SZ; i++)
    {
        sp_blk.free_blks_lst[i] = 1;
    }

    for (i = 0; i < SP_INODES_SZ; i++)
    {
        sp_blk.free_inodes_lst[i] = 1;
    }

    super_read();*/

  //  inode_lst_init();
    inode_incore_init();



   // data_blk_init();
     disk_read(free_db_hd_lst, sp_blk.free_blks_hd, 1);

  //  my_sys_mkroot();


    fprintf(stderr, "fs_init ok!\n");
}

int sync()
{
    int ret1 = super_write();
    int ret2 = disk_write(free_db_hd_lst, sp_blk.free_blks_hd, 1);


    return ret1&&ret2;
}

static void* my_init(struct fuse_conn_info *conn)
{
    log_print("my Inited!\n");

    return (FILE*)fuse_get_context()->private_data;
}

static int my_statfs(const char *path, struct statvfs *statvfs_buf) {
    statvfs_buf->f_bsize = BLOCK_SIZE;         // 文件系统块大小
    statvfs_buf->f_frsize = BLOCK_SIZE;        // 分片大小
    statvfs_buf->f_blocks = BLK_NUM;      // 文件系统中的总块数
    statvfs_buf->f_bfree = BLK_NUM;        // 可用块数
    statvfs_buf->f_bavail = BLK_NUM;       // 非超级用户可获取的块数
    statvfs_buf->f_files = MAX_OPEN_FILES;       // 文件系统中的总文件数
    statvfs_buf->f_ffree = MAX_OPEN_FILES;        // 可用的文件数
    statvfs_buf->f_favail = MAX_OPEN_FILES;       // 非超级用户可获取的文件数
    statvfs_buf->f_fsid = "mrlonely";               // 文件系统标识
    statvfs_buf->f_flag = 0;                        // 挂载标志
    statvfs_buf->f_namemax = MAX_FNAME_LEN; // 最大文件名长度

    return 0;
}

static int my_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
    log_print("Getattr!\n");
    log_print(path);
    log_print("\n");

	memset(stbuf, 0, sizeof(struct stat));
    /*if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
    } else if (strcmp(path, my_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
        stbuf->st_size = strlen(my_str);
	} else
        res = -ENOENT;*/

    inode_incore* pinode = namei(path);
    if (!pinode)
        return -ENOENT;

    file_type inode_type = pinode->inode.itype;
    mode_t perm = pinode->inode.perms;
    stbuf->st_nlink = pinode->inode.link_num;

    if (inode_type == 1) {//Regular file
        stbuf->st_mode = S_IFREG | perm;
        stbuf->st_size = pinode->inode.file_sz;
    } else if(inode_type == 2) {//Directory
        stbuf->st_mode = S_IFDIR | perm;
    } else
        res = -ENOENT;


    stbuf->st_atime = pinode->inode.i_tv[0].tv_sec;
   // stbuf->st_atime_usec = pinode->inode.i_tv[0].tv_nsec;
    stbuf->st_mtime = pinode->inode.i_tv[1].tv_sec;
   // stbuf->st_mtime_usec = pinode->inode.i_tv[1].tv_nsec;

	iput(pinode);


	return res;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
    log_print("Readdir!\n");
	(void) offset;
	(void) fi;

    //if (strcmp(path, "/") != 0)
    //	return -ENOENT;
    inode_incore* pinode = namei(path);

    if (!pinode)
        return -ENOENT;

    file_type inode_type = pinode->inode.itype;

    if (inode_type != 2) {
	iput(pinode);
        return -ENOENT;
    }

    dpackage * dpk = calloc(1, sizeof(dpackage));
    dir_ent* next_dir = dirent_get(pinode, dpk);

    while (next_dir) {
        const char* file_name = next_dir->d_name;
        filler(buf, file_name, NULL, 0, 0);
	next_dir = dirent_get(NULL, dpk);
    }

    free(dpk);
/*    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    filler(buf, my_path + 1, NULL, 0);
 */
	pinode->inode.i_tv[0].tv_sec = time(NULL);
	pinode->status |= 0x04;
	iput(pinode);
	return 0;
}

static int my_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
   /* inode_incore* opened_inode = namei(path);

    if(inode_incore) {

    }   */

    log_print("Create!\n");

    fi->fh = my_sys_open(path, fi->flags, mode);

    return 0;
}

static int my_open(const char *path, struct fuse_file_info *fi)
{
    log_print("Open!\n");



  //  if (strcmp(path, my_path) != 0)
//		return -ENOENT;

    //if ((fi->flags & 3) != O_RDONLY)
     //   return -EACCES;


    fi->fh = my_sys_open(path, fi->flags, 0);

    return 0;
}

static int my_release(const char *path, struct fuse_file_info *fi)
{
    inode_incore* pinode = (inode_incore*)(fi->fh);

    if( !pinode)
	return 0;

    pthread_mutex_lock(&(pinode->lock));

    pinode->status |= 0x04;
    iput(pinode);

    return 0;
}


static int my_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
    log_print("Read!\n");

/*	size_t len;
	(void) fi;
    if(strcmp(path, my_path) != 0)
		return -ENOENT;

    len = strlen(my_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
        memcpy(buf, my_str + offset, size);
	} else
        size = 0;*/

    //return size;
    inode_incore* pinode = (inode_incore*)(fi->fh);

    if (!pinode)
        return -1;

    pthread_mutex_lock(&(pinode->lock));

    int res =  my_sys_read(fi->fh, buf, size, offset);

    pthread_mutex_unlock(&(pinode->lock));

    return res;
}

static int my_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi)
{
    log_print("write!\n");

    inode_incore* pinode = (inode_incore*)(fi->fh);

    if (!pinode)
        return -1;

    pthread_mutex_lock(&(pinode->lock));

    int res =  my_sys_write(fi->fh, buf, size, offset);

    pthread_mutex_unlock(&(pinode->lock));

    return res;
}

static int my_truncate(const char *path, off_t size)
{
    inode_incore* pinode = namei(path);
    file_free(pinode);
    pinode->status |= 0x04;
    iput(pinode);

    return 0;
}

static void my_destroy(void *userdata)
{

    sync();

    //free(disk);
    close(disk_fd);

    log_print("Sync() called!\n");
    log_print("Disk Freed!\n");
    fclose(LOG_FILE);
}

static int my_access(const char * path, int mask)
{
	int res;
    log_print("access!\n");

    res = my_sys_access(path, mask);

	if (res == -1)
		return -errno;
 
	return 0;
}

static int my_mkdir(const char * path, mode_t mode)
{
    log_print("mkdir!\n");

	if(my_sys_mkdir(path, mode))
		return -1;
	return 0;
}

static int my_unlink(const char * path)
{
    log_print("unlink!\n");
	if(my_sys_unlink(path))
		return -EISDIR;
	return 0;
}

static int my_utimens(const char *path, const struct timespec tv[2])
{
    log_print("utimes!\n");

    inode_incore* pinode = namei(path);
    pinode->inode.i_tv[0] = tv[0];
    pinode->inode.i_tv[1] = tv[1];
    pinode->status |= 0x04;
    iput(pinode);

    return 0;
}

static int my_rmdir(const char * path)
{
	int res;
	res = my_sys_rmdir(path);
	return res;

}

static int my_chown(const char * path, uid_t user_id, gid_t group_id)
{
	inode_incore * incore = namei(path);
	if(! incore)
		return -1;

	incore->inode.user_id = user_id;
	incore->inode.group_id = group_id;
	incore->status |= 0x04;

	iput(incore);

	return 0;
}

static int my_chmod(const char * path, mode_t mode)
{
	inode_incore * incore = namei(path);

	if(! incore)
		return -1;

	incore->inode.perms = mode;
	incore->status |= 0x04;

	iput(incore);

	return 0;
}

static int my_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
   // inode_incore* pinode = (inode_incore*)(fi->fh);


    return 0;
}

static struct fuse_operations my_oper = {
    .init       = my_init,
    .getattr	= my_getattr,
    .readdir	= my_readdir,
    .open       = my_open,
    .release    = my_release,
    .create     = my_create,
    .read       = my_read,
    .write      = my_write,
    .truncate   = my_truncate,
    .destroy    = my_destroy,
    .mkdir      = my_mkdir,
    .unlink     = my_unlink,
    .access     = my_access,
    .chmod	= my_chmod,
    .chown	= my_chown,
    .utimens    = my_utimens,
    .rmdir      = my_rmdir,
    .fsync      = my_fsync,
};

int main(int argc, char *argv[])
{
    if ((argc < 3)||(argv[argc-2][0] == '-') || (argv[argc-1][0] == '-')) {
        fprintf(stderr, "Last two argument must be path of device and path of mount point\n");
        return -1;
    }


    char* disk_path = realpath(argv[argc-2], NULL);

    if (!disk_path) {
        fprintf(stderr, "Disk path is not correct!\n");
        return -1;
    }
    fprintf(stderr, "Disk path: %s\n", disk_path);

    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    if (load_disk(disk_path) == -1)
        return -1;

 //   mkfs();
    char flag_string[BLOCK_SIZE];
    disk_read(flag_string, 0, 1);

    if (strncmp(flag_string, "my", strlen("my"))) {
        fprintf(stderr, "Call my_mkfs first!\n");
        close(disk_fd);
        return -1;
    }

    fs_init();

    fprintf(stderr, "DISK_SIZE = %ld\n", DISK_SIZE);

    fprintf(stderr, "BLOCK_SIZE = %ld\n", BLOCK_SIZE);

    //dir_test();


  //  dump_disk("disk_final");
    FILE* log = fopen("fs.log", "w");
    setvbuf(log, NULL, _IOLBF, 0);


  //  test_data_blk();
  //  test_bmap();

    int ret = fuse_main(argc, argv, &my_oper, log);


    return ret;
}
