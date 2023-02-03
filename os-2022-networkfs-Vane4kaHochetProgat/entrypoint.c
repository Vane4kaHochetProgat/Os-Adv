#include "entrypoint.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Murashov Ivan");
MODULE_VERSION("0.01");

int networkfs_createdir(struct user_namespace *ignored,
                        struct inode *parent_inode, struct dentry *child_dentry,
                        umode_t mode) {
  char networkfs_http_call_buffer[8];
  char parent_for_request[8];
  char name_for_request[NAME_SIZE];

  sprintf(parent_for_request, "%ld", parent_inode->i_ino);
  url_encode(child_dentry->d_name.name, name_for_request,
             strlen(child_dentry->d_name.name));

  int err = networkfs_http_call(parent_inode->i_sb->s_fs_info, "create",
                                networkfs_http_call_buffer, sizeof(ino_t), 3,
                                "parent", (char *)parent_for_request, "name",
                                (char *)name_for_request, "type", "directory");
  ino_t *inot = (ino_t *)networkfs_http_call_buffer;
  if (err != 0) {
    printk(KERN_INFO "mkdir error: %d\n", err);
    return 1;
  }

  struct inode *inode =
      networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, *inot);
  d_add(child_dentry, inode);

  printk(KERN_INFO "folder created successfully");
  return 0;
}

int networkfs_unlinkdir(struct inode *parent_inode,
                        struct dentry *child_dentry) {
  char networkfs_http_call_buffer[4];
  char request_buffer[3][NAME_SIZE];
  const char *request_params[2];
  request_params[0] = request_buffer[0];
  request_params[1] = request_buffer[1];

  sprintf(request_buffer[0], "%ld", parent_inode->i_ino);
  sprintf(request_buffer[2], "%s", child_dentry->d_name.name);
  url_encode(request_buffer[2], request_buffer[1], strlen(request_buffer[2]));

  int err = networkfs_http_call(parent_inode->i_sb->s_fs_info, "rmdir",
                                (char *)networkfs_http_call_buffer, sizeof(int),
                                2, "parent", request_params[0], "name",
                                request_params[1]);

  if (err != 0) {
    printk(KERN_INFO "unlinkdir error: %d\n", err);
    return 1;
  }

  printk(KERN_INFO "folder deleted successfully");
  return 0;
}

struct dentry *networkfs_lookup(struct inode *parent_inode,
                                struct dentry *child_dentry,
                                unsigned int flag) {
  char *networkfs_http_call_buffer =
      kmalloc(sizeof(struct entry_info), GFP_KERNEL);
  char request_buffer[3][NAME_SIZE];
  const char *request_params[2];
  request_params[0] = request_buffer[0];
  request_params[1] = request_buffer[1];

  sprintf(request_buffer[0], "%ld", parent_inode->i_ino);
  sprintf(request_buffer[2], "%s", child_dentry->d_name.name);
  url_encode(request_buffer[2], request_buffer[1], strlen(request_buffer[2]));

  int err = networkfs_http_call(parent_inode->i_sb->s_fs_info, "lookup",
                                (char *)networkfs_http_call_buffer,
                                sizeof(struct entry_info), 2, "parent",
                                request_params[0], "name", request_params[1]);
  struct entry_info *info = (struct entry_info *)networkfs_http_call_buffer;

  if (err != 0) {
    printk(KERN_INFO "lookup error: %d\n", err);
    kfree(networkfs_http_call_buffer);
    return NULL;
  }

  struct inode *inode;
  if (info->entry_type == 4) {
    inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFDIR, info->ino);
    d_add(child_dentry, inode);
  } else if (info->entry_type == 8) {
    inode = networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, info->ino);
    d_add(child_dentry, inode);
  }
  printk(KERN_INFO "lookup ended");
  kfree(networkfs_http_call_buffer);
  return NULL;
}

int networkfs_iterate(struct file *filp, struct dir_context *ctx) {
  char *networkfs_http_call_buffer =
      kmalloc(sizeof(struct entries), GFP_KERNEL);
  char networkfs_name_buffer[3 * NAME_SIZE];
  unsigned long offset;
  int stored;
  unsigned char ftype;
  ino_t ino;
  struct dentry *dentry = filp->f_path.dentry;
  struct inode *inode = dentry->d_inode;

  const char *request_param;
  char request_buffer[4];
  sprintf(request_buffer, "%ld", inode->i_ino);
  request_param = request_buffer;

  int err = networkfs_http_call(
      inode->i_sb->s_fs_info, "list", (char *)networkfs_http_call_buffer,
      sizeof(struct entries), 1, "inode", request_param);
  if (err != 0) {
    printk(KERN_INFO "iterate error: %d\n", err);
    kfree(networkfs_http_call_buffer);
    return -1;
  }

  struct entries *entries = (struct entries *)networkfs_http_call_buffer;
  offset = ctx->pos;
  stored = offset;
  while (stored < entries->entries_count + 2) {
    if (stored == 0) {
      strcpy(networkfs_name_buffer, ".");
      ftype = DT_DIR;
      ino = inode->i_ino;
    } else if (stored == 1) {
      strcpy(networkfs_name_buffer, "..");
      ftype = DT_DIR;
      ino = dentry->d_parent->d_inode->i_ino;
    } else {
      strcpy(networkfs_name_buffer, entries->entries[stored - 2].name);
      ftype = entries->entries[stored - 2].entry_type;
      ino = entries->entries[stored - 2].ino;
    }
    dir_emit(ctx, networkfs_name_buffer, strlen(networkfs_name_buffer), ino,
             ftype);
    stored++;
    offset++;
    ctx->pos = offset;
  }
  printk(KERN_INFO "iteration ended");
  int result = entries->entries_count - ctx->pos + 2;
  kfree(networkfs_http_call_buffer);
  return result;
}

int networkfs_create(struct user_namespace *ignored, struct inode *parent_inode,
                     struct dentry *child_dentry, umode_t mode, bool b) {
  char networkfs_http_call_buffer[8];
  char request_buffer[3][NAME_SIZE];
  const char *request_params[2];
  request_params[0] = request_buffer[0];
  request_params[1] = request_buffer[1];

  sprintf(request_buffer[0], "%ld", parent_inode->i_ino);
  sprintf(request_buffer[2], "%s", child_dentry->d_name.name);
  url_encode(request_buffer[2], request_buffer[1], strlen(request_buffer[2]));

  int err = networkfs_http_call(parent_inode->i_sb->s_fs_info, "create",
                                (char *)networkfs_http_call_buffer,
                                sizeof(ino_t), 3, "parent", request_params[0],
                                "name", request_params[1], "type", "file");
  if (err != 0) {
    printk(KERN_INFO "create error: %d\n", err);
    return 1;
  }
  ino_t *ino = (ino_t *)networkfs_http_call_buffer;

  struct inode *inode =
      networkfs_get_inode(parent_inode->i_sb, NULL, S_IFREG, *ino);
  d_add(child_dentry, inode);

  printk(KERN_INFO "file created successfully\n");
  return 0;
}

int networkfs_unlink(struct inode *parent_inode, struct dentry *child_dentry) {
  char networkfs_http_call_buffer[4];
  char request_buffer[3][NAME_SIZE];
  const char *request_params[2];
  request_params[0] = request_buffer[0];
  request_params[1] = request_buffer[1];

  sprintf(request_buffer[0], "%ld", parent_inode->i_ino);
  sprintf(request_buffer[2], "%s", child_dentry->d_name.name);
  url_encode(request_buffer[2], request_buffer[1], strlen(request_buffer[2]));
  int err =
      networkfs_http_call(parent_inode->i_sb->s_fs_info, "unlink",
                          (char *)networkfs_http_call_buffer, 4, 2, "parent",
                          request_params[0], "name", request_params[1]);
  if (err != 0) {
    printk(KERN_INFO "unlink error: %d\n", err);
    return 1;
  }

  return 0;
}

struct inode *networkfs_get_inode(struct super_block *sb,
                                  const struct inode *dir, umode_t mode,
                                  int i_ino) {
  struct inode *inode;
  inode = new_inode(sb);
  inode->i_ino = i_ino;
  inode->i_op = &networkfs_inode_ops;
  if (mode & S_IFREG) {
    inode->i_fop = &networkfs_file_ops;
  } else {
    inode->i_fop = &networkfs_dir_ops;
  }
  if (inode != NULL) {
    // S_IRWXUGO is 777 all-permissive code mode
    inode_init_owner(&init_user_ns, inode, dir, mode | S_IRWXUGO);
  }
  return inode;
}

int networkfs_fill_super(struct super_block *sb, void *ignored_data,
                         int ignored) {
  struct inode *inode;
  inode = networkfs_get_inode(sb, NULL, S_IFDIR, 1000);
  sb->s_root = d_make_root(inode);
  if (sb->s_root == NULL) {
    return -ENOMEM;
  }
  printk(KERN_INFO "fill_super returned 0\n");
  return 0;
}

void networkfs_kill_sb(struct super_block *sb) {
  kfree(sb->s_fs_info);
  printk(KERN_INFO
         "networkfs super block is destroyed. Unmount successfully.\n");
}

struct dentry *networkfs_mount(struct file_system_type *fs_type, int flags,
                               const char *init_token, void *data) {
  char *token = kmalloc(TOKEN_SIZE, GFP_KERNEL);
  strcpy(token, init_token);
  struct dentry *ret;
  ret = mount_nodev(fs_type, flags, data, networkfs_fill_super);
  if (ret == NULL) {
    printk(KERN_ERR "Can't mount file system");
  } else {
    printk(KERN_INFO "Mounted successfuly");
  }
  ret->d_sb->s_fs_info = token;
  return ret;
}

int networkfs_init(void) {
  printk(KERN_INFO "Hello, World!\n");
  int status = register_filesystem(&networkfs_fs_type);
  printk(KERN_INFO "register exit code: %d\n", status);
  return 0;
}

void networkfs_exit(void) {
  int status = unregister_filesystem(&networkfs_fs_type);
  printk(KERN_INFO "unregister exit code: %d\n", status);
  printk(KERN_INFO "Goodbye!\n");
}

void url_encode(const unsigned char *orig, char *enc, int len) {
  const char *hex = "0123456789abcdef";

  int pos = 0;
  for (int i = 0; i < len; i++) {
    if (('a' <= orig[i] && orig[i] <= 'z') ||
        ('A' <= orig[i] && orig[i] <= 'Z') ||
        ('0' <= orig[i] && orig[i] <= '9')) {
      enc[pos++] = orig[i];
    } else {
      enc[pos++] = '%';
      enc[pos++] = hex[orig[i] >> 4];
      enc[pos++] = hex[orig[i] & 15];
    }
  }
  enc[pos] = '\0';
}

ssize_t networkfs_read(struct file *filp, char *buffer, size_t len,
                       loff_t *offset) {
  char *networkfs_http_call_buffer = kmalloc(FILE_SIZE, GFP_KERNEL);
  printk(KERN_INFO "read start\n");
  char request_inode[8];
  sprintf(request_inode, "%ld", filp->f_inode->i_ino);
  int err = networkfs_http_call(filp->f_inode->i_sb->s_fs_info, "read",
                                networkfs_http_call_buffer, FILE_SIZE, 1,
                                "inode", request_inode);
  if (err != 0) {
    printk(KERN_INFO "read error exit code is: %d\n", err);
    kfree(networkfs_http_call_buffer);
    return 0;
  }
  struct file_content *content =
      (struct file_content *)networkfs_http_call_buffer;
  long rest = 0;
  if (len + *offset < content->content_length) {
    rest = len;
  } else {
    rest = content->content_length - *offset;
  }

  long wrote = __copy_to_user(buffer, content->content + *offset, rest);

  if (wrote != 0) {
    printk(KERN_INFO "can't write\n");
    kfree(networkfs_http_call_buffer);
    return wrote;
  }
  kfree(networkfs_http_call_buffer);
  (*offset) += (rest - wrote);
  return rest - wrote;
}
ssize_t networkfs_write(struct file *filp, const char *buffer, size_t len,
                        loff_t *offset) {
  char *networkfs_http_call_buffer = kmalloc(FILE_SIZE, GFP_KERNEL);
  char TMP_BUFF[8];
  char request_inode[8];
  char *request_content = kmalloc(FILE_SIZE, GFP_KERNEL);
  sprintf(request_inode, "%ld", filp->f_inode->i_ino);

  printk(KERN_INFO "len: %ld\n", len);
  __copy_from_user(networkfs_http_call_buffer, buffer + *offset, len);
  url_encode(networkfs_http_call_buffer, request_content, len);
  int err = networkfs_http_call(filp->f_inode->i_sb->s_fs_info, "write",
                                (char *)TMP_BUFF, 8, 2, "inode", request_inode,
                                "content", request_content);
  kfree(request_content);
  if (err != 0) {
    printk(KERN_INFO "read error exit code is: %d\n", err);
    kfree(networkfs_http_call_buffer);
    return 0;
  }

  (*offset) += (len);
  kfree(networkfs_http_call_buffer);
  return len;
}

int networkfs_link(struct dentry *old_dentry, struct inode *parent_dir,
                   struct dentry *new_dentry) {
  char networkfs_http_call_buffer[8];
  char request_name[NAME_SIZE];
  char request_source[8];
  char request_parent[8];

  sprintf(request_source, "%ld", old_dentry->d_inode->i_ino);
  sprintf(request_parent, "%ld", parent_dir->i_ino);
  sprintf(request_name, "%s", new_dentry->d_name.name);
  int err = networkfs_http_call(parent_dir->i_sb->s_fs_info, "link",
                                (char *)networkfs_http_call_buffer, 8, 3,
                                "source", request_source, "parent",
                                request_parent, "name", request_name);
  if (err != 0) {
    printk(KERN_INFO "error in link: %d", err);
  }

  return err;
}

module_init(networkfs_init);
module_exit(networkfs_exit);