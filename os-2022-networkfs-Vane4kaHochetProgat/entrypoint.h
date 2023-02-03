#ifndef NETWORKFS_ENTRYPOINT
#define NETWORKFS_ENTRYPOINT
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/uaccess.h>

#include "http.h"

#define TOKEN_SIZE 37
#define BUFFER_SIZE 4360
#define NAME_SIZE 256
#define FILE_SIZE 1024

struct file_content {
  u64 content_length;
  char content[];
};

struct entry_info {
  unsigned char entry_type;
  ino_t ino;
};

struct entries {
  size_t entries_count;
  struct entry {
    unsigned char entry_type;
    ino_t ino;
    char name[256];
  } entries[16];
};

int networkfs_createdir(struct user_namespace *, struct inode *,
                        struct dentry *, umode_t);

int networkfs_unlinkdir(struct inode *, struct dentry *);

struct dentry *networkfs_lookup(struct inode *, struct dentry *, unsigned int);

int networkfs_iterate(struct file *, struct dir_context *);

int networkfs_create(struct user_namespace *, struct inode *, struct dentry *,
                     umode_t, bool);

int networkfs_unlink(struct inode *, struct dentry *);

struct inode *networkfs_get_inode(struct super_block *, const struct inode *,
                                  umode_t, int);

int networkfs_fill_super(struct super_block *, void *, int);

void networkfs_kill_sb(struct super_block *);

struct dentry *networkfs_mount(struct file_system_type *, int, const char *,
                               void *);

ssize_t networkfs_read(struct file *, char *, size_t, loff_t *);
ssize_t networkfs_write(struct file *, const char *, size_t, loff_t *);

int networkfs_link(struct dentry *, struct inode *, struct dentry *);

int networkfs_init(void);

void networkfs_exit(void);

struct inode_operations networkfs_inode_ops = {
    .lookup = networkfs_lookup,
    .create = networkfs_create,
    .unlink = networkfs_unlink,
    .mkdir = networkfs_createdir,
    .rmdir = networkfs_unlinkdir,
    .link = networkfs_link,
};

struct file_operations networkfs_dir_ops = {
    .iterate = networkfs_iterate,
};

struct file_operations networkfs_file_ops = {
    .read = networkfs_read,
    .write = networkfs_write,
};

struct file_system_type networkfs_fs_type = {.name = "networkfs",
                                             .mount = networkfs_mount,
                                             .kill_sb = networkfs_kill_sb};

void url_encode(const unsigned char *, char *, int);

#endif