#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Viktor Prutyanov");
MODULE_VERSION("0.1");

#define VPFS_BLOCKSIZE_BITS 12
#define VPFS_BLOCKSIZE (1 << VPFS_BLOCKSIZE_BITS)
#define VPFS_MAGIC 0xbeef1337

static int vpfs_mknod(struct inode *dir, struct dentry *dentry,
        umode_t mode, dev_t dev);
static int vpfs_create(struct inode *dir, struct dentry *dentry,
        umode_t mode, bool excl);

static const struct super_operations vpfs_super_ops = {
    .statfs = simple_statfs,
};

static const struct inode_operations vpfs_dir_inode_ops = {
    .create = vpfs_create,
    .lookup = simple_lookup,
};

static const struct file_operations vpfs_file_ops = {
    .read_iter = generic_file_read_iter,
    .write_iter = generic_file_write_iter,
    .llseek = generic_file_llseek,
};

static const struct inode_operations vpfs_file_inode_ops = {
    .getattr        = simple_getattr, /* For correct 'blocks' field in stat */
};

static const struct address_space_operations vpfs_as_ops = {
    .readpage       = simple_readpage,
    .write_begin    = simple_write_begin,
    .write_end      = simple_write_end,
};

struct inode *vpfs_get_inode(struct super_block *sb, const struct inode *dir,
        int mode)
{
    struct inode *inode = new_inode(sb);

    if (!inode) {
        return NULL;
    }

    inode_init_owner(inode, dir, mode); /* Init uid, gid, mode */
    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
    inode->i_ino = get_next_ino();
    inode->i_mapping->a_ops = &vpfs_as_ops;

    if (S_ISDIR(mode)) {
        inode->i_fop = &simple_dir_operations;
        inode->i_op = &vpfs_dir_inode_ops;
        inc_nlink(inode);
    }

    if (S_ISREG(mode)) {
        inode->i_op = &vpfs_file_inode_ops;
        inode->i_fop = &vpfs_file_ops;
    }

    return inode;
}

static int vpfs_mknod(struct inode *dir, struct dentry *dentry,
        umode_t mode, dev_t dev)
{
    struct inode *inode = vpfs_get_inode(dir->i_sb, dir, mode);

    if (!inode) {
        return -ENOSPC;
    }

    d_instantiate(dentry, inode);
    dget(dentry);
    dir->i_mtime = dir->i_ctime = current_time(inode);

    return 0;
}

static int vpfs_create(struct inode *dir, struct dentry *dentry,
        umode_t mode, bool excl)
{
    return vpfs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int vpfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct inode *root_inode;
    struct dentry *root_dentry;

    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = VPFS_BLOCKSIZE;
    sb->s_blocksize_bits = VPFS_BLOCKSIZE_BITS;
    sb->s_magic = VPFS_MAGIC;
    sb->s_op = &vpfs_super_ops;

    root_inode = vpfs_get_inode(sb, NULL, S_IFDIR | 0755);
    if (!root_inode) {
        return -ENOMEM;
    }

    root_dentry = d_make_root(root_inode);
    if (!root_dentry) {
        goto out_root;
    }
    sb->s_root = root_dentry;

    return 0;

out_root:
    iput(root_inode);

    return -ENOMEM;
}

static struct dentry *vpfs_mount(struct file_system_type *fs_type, int flags,
        const char *dev_name, void *data)
{
    return mount_nodev(fs_type, flags, data, vpfs_fill_super);
}

static struct file_system_type vpfs_fs_type = {
    .owner = THIS_MODULE,
    .name = "vpfs",
    .mount = vpfs_mount,
    .kill_sb = kill_litter_super,
};

static int __init vpfs_init(void)
{
    int err;

    err = register_filesystem(&vpfs_fs_type);
    if (err) {
        pr_alert("vpfs: register_filesystem failed\n");

        return err;
    }

    pr_info("vpfs: module loaded\n");

    return 0;
}

static void __exit vpfs_exit(void)
{
    unregister_filesystem(&vpfs_fs_type);
    pr_info("vpfs: module unloaded\n");
}

module_init(vpfs_init);
module_exit(vpfs_exit);
