/* Read-only FAT16 / FAT32 driver.  8.3 (short) names only; long names are
 * skipped, but every file also has a short name you can use. */
#ifndef FAT_H
#define FAT_H
#include <stdint.h>
#include <stdbool.h>

#define FAT_OK        0
#define FAT_EIO      (-1)
#define FAT_ENOENT   (-2)
#define FAT_ENOTDIR  (-3)
#define FAT_EFMT     (-4)
#define FAT_EISDIR   (-5)

#define FAT_ATTR_DIR 0x10

typedef int (*fat_read_fn)(uint32_t lba, void *buf512);   /* 0 = ok */

typedef struct {
    fat_read_fn rd;
    uint32_t part_lba, fat_lba, root_lba, root_sectors, data_lba, root_cluster, clusters;
    uint32_t cur_lba;
    bool     cur_valid;
    uint8_t  spc, type;                 /* sectors per cluster, 16 or 32 */
    uint8_t  buf[512];
} fat_fs_t;

typedef struct {
    uint32_t first_cluster, size, pos, cur_cluster, cur_index;
    bool is_dir;
} fat_file_t;

typedef void (*fat_list_cb)(const char *name, uint32_t size, bool is_dir, void *ctx);

int fat_mount(fat_fs_t *fs, fat_read_fn rd);
int fat_open(fat_fs_t *fs, const char *path, fat_file_t *f);
int fat_read(fat_fs_t *fs, fat_file_t *f, void *dst, uint32_t n);   /* bytes read or <0 */
int fat_list(fat_fs_t *fs, const char *path, fat_list_cb cb, void *ctx);
const char *fat_strerror(int err);
#endif
