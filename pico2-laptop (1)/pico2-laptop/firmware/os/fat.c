#include "fat.h"
#include <string.h>

typedef struct { char name[13]; uint8_t attr; uint32_t cluster; uint32_t size; } dirent_t;
typedef int (*visit_fn)(const dirent_t *e, void *ctx);   /* nonzero = stop */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const uint8_t *load(fat_fs_t *fs, uint32_t lba)
{
    if (fs->cur_valid && fs->cur_lba == lba) return fs->buf;
    fs->cur_valid = false;
    if (fs->rd(lba, fs->buf) != 0) return NULL;
    fs->cur_lba = lba; fs->cur_valid = true;
    return fs->buf;
}

const char *fat_strerror(int err)
{
    switch (err) {
    case FAT_OK: return "ok";
    case FAT_EIO: return "disk read error";
    case FAT_ENOENT: return "no such file or directory";
    case FAT_ENOTDIR: return "not a directory";
    case FAT_EFMT: return "not a FAT16/FAT32 volume";
    case FAT_EISDIR: return "is a directory";
    default: return "error";
    }
}

int fat_mount(fat_fs_t *fs, fat_read_fn rd)
{
    memset(fs, 0, sizeof *fs);
    fs->rd = rd;
    const uint8_t *b = load(fs, 0);
    if (!b) return FAT_EIO;
    uint32_t part = 0;
    int is_bpb = (b[0] == 0xEB || b[0] == 0xE9) && rd16(b + 11) == 512;
    if (b[510] != 0x55 || b[511] != 0xAA) return FAT_EFMT;
    if (!is_bpb) {                                    /* MBR: first FAT partition */
        int found = 0;
        for (int i = 0; i < 4; i++) {
            const uint8_t *e = b + 446 + 16 * i;
            uint8_t t = e[4];
            if (t == 0x04 || t == 0x06 || t == 0x0B || t == 0x0C || t == 0x0E) {
                part = rd32(e + 8); found = 1; break;
            }
        }
        if (!found) return FAT_EFMT;
        b = load(fs, part);
        if (!b) return FAT_EIO;
        if (rd16(b + 11) != 512) return FAT_EFMT;
    }
    uint8_t  spc = b[13], nfats = b[16];
    uint32_t reserved = rd16(b + 14), root_entries = rd16(b + 17);
    uint32_t tot = rd16(b + 19) ? rd16(b + 19) : rd32(b + 32);
    uint32_t fatsz = rd16(b + 22) ? rd16(b + 22) : rd32(b + 36);
    uint32_t root_clus = rd32(b + 44);
    if (!spc || !nfats || !fatsz) return FAT_EFMT;

    uint32_t root_sectors = (root_entries * 32u + 511u) / 512u;
    uint32_t first_data = reserved + nfats * fatsz + root_sectors;
    if (tot <= first_data) return FAT_EFMT;
    uint32_t clusters = (tot - first_data) / spc;
    if (clusters < 4085) return FAT_EFMT;             /* FAT12: not supported */

    fs->type = clusters < 65525 ? 16 : 32;
    fs->spc = spc;
    fs->part_lba = part;
    fs->fat_lba = part + reserved;
    fs->root_lba = part + reserved + nfats * fatsz;
    fs->root_sectors = root_sectors;
    fs->data_lba = part + first_data;
    fs->root_cluster = fs->type == 32 ? root_clus : 0;
    fs->clusters = clusters;
    return FAT_OK;
}

static uint32_t cluster_lba(const fat_fs_t *fs, uint32_t cl)
{
    return fs->data_lba + (cl - 2u) * fs->spc;
}

static int next_cluster(fat_fs_t *fs, uint32_t cl, uint32_t *out)
{
    uint32_t off = fs->type == 32 ? cl * 4u : cl * 2u;
    const uint8_t *b = load(fs, fs->fat_lba + off / 512u);
    if (!b) return FAT_EIO;
    *out = fs->type == 32 ? (rd32(b + off % 512u) & 0x0FFFFFFFu) : rd16(b + off % 512u);
    return FAT_OK;
}

static bool is_end(const fat_fs_t *fs, uint32_t v)
{
    return v < 2 || (fs->type == 32 ? v >= 0x0FFFFFF7u : v >= 0xFFF7u);
}

static char lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static void make_name(const uint8_t *e, char *out)
{
    int n = 0;
    int lbase = (e[12] & 0x08) != 0, lext = (e[12] & 0x10) != 0;
    for (int i = 0; i < 8 && e[i] != ' '; i++) {
        char c = (char)((i == 0 && e[0] == 0x05) ? 0xE5 : e[i]);
        out[n++] = lbase ? lower(c) : c;
    }
    if (e[8] != ' ') {
        out[n++] = '.';
        for (int i = 8; i < 11 && e[i] != ' '; i++) out[n++] = lext ? lower((char)e[i]) : (char)e[i];
    }
    out[n] = 0;
}

/* 1 = stop walking (visitor said so, or end-of-directory marker), 0 = go on, <0 = error */
static int walk_sector(fat_fs_t *fs, uint32_t lba, visit_fn v, void *ctx)
{
    const uint8_t *b = load(fs, lba);
    if (!b) return FAT_EIO;
    for (int i = 0; i < 16; i++) {
        const uint8_t *e = b + i * 32;
        if (e[0] == 0x00) return 1;                   /* end of directory */
        if (e[0] == 0xE5) continue;                   /* deleted */
        if (e[11] == 0x0F) continue;                  /* long-name piece */
        if (e[11] & 0x08) continue;                   /* volume label */
        dirent_t d;
        make_name(e, d.name);
        d.attr = e[11];
        d.cluster = ((fs->type == 32) ? ((uint32_t)rd16(e + 20) << 16) : 0u) | rd16(e + 26);
        d.size = rd32(e + 28);
        if (v(&d, ctx)) return 1;
    }
    return 0;
}

static int dir_walk(fat_fs_t *fs, uint32_t dir_cl, visit_fn v, void *ctx)
{
    if (dir_cl == 0 && fs->type == 16) {              /* FAT16 fixed root directory */
        for (uint32_t s = 0; s < fs->root_sectors; s++) {
            int r = walk_sector(fs, fs->root_lba + s, v, ctx);
            if (r) return r < 0 ? r : 0;
        }
        return 0;
    }
    uint32_t cl = dir_cl ? dir_cl : fs->root_cluster;
    for (uint32_t guard = 0; guard < 65536u; guard++) {
        for (uint32_t s = 0; s < fs->spc; s++) {
            int r = walk_sector(fs, cluster_lba(fs, cl) + s, v, ctx);
            if (r) return r < 0 ? r : 0;
        }
        uint32_t nx;
        int r = next_cluster(fs, cl, &nx);
        if (r < 0) return r;
        if (is_end(fs, nx)) break;
        cl = nx;
    }
    return 0;
}

static bool ieq(const char *a, const char *b)
{
    while (*a && *b) { if (lower(*a) != lower(*b)) return false; a++; b++; }
    return *a == *b;
}

typedef struct { const char *want; dirent_t found; bool ok; } find_ctx;
static int find_visit(const dirent_t *e, void *c)
{
    find_ctx *f = c;
    if (ieq(e->name, f->want)) { f->found = *e; f->ok = true; return 1; }
    return 0;
}

int fat_open(fat_fs_t *fs, const char *path, fat_file_t *f)
{
    uint32_t cl = fs->root_cluster, size = 0;
    bool is_dir = true;
    const char *p = path;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char comp[13];
        int n = 0;
        while (*p && *p != '/') { if (n < 12) comp[n++] = *p; p++; }
        comp[n] = 0;
        if (!is_dir) return FAT_ENOTDIR;
        if (!strcmp(comp, ".")) continue;
        if (!strcmp(comp, "..") && cl == fs->root_cluster) continue;
        find_ctx fc = { comp, {{0}, 0, 0, 0}, false };
        int r = dir_walk(fs, cl, find_visit, &fc);
        if (r < 0) return r;
        if (!fc.ok) return FAT_ENOENT;
        is_dir = (fc.found.attr & FAT_ATTR_DIR) != 0;
        cl = fc.found.cluster;
        if (is_dir && cl == 0) cl = fs->root_cluster;
        size = is_dir ? 0 : fc.found.size;
    }
    f->first_cluster = cl; f->size = size; f->pos = 0;
    f->cur_cluster = 0; f->cur_index = 0; f->is_dir = is_dir;
    return FAT_OK;
}

int fat_read(fat_fs_t *fs, fat_file_t *f, void *dst, uint32_t n)
{
    if (f->is_dir) return FAT_EISDIR;
    if (f->pos >= f->size || f->first_cluster < 2) return 0;
    if (n > f->size - f->pos) n = f->size - f->pos;
    uint32_t cbytes = (uint32_t)fs->spc * 512u, done = 0;
    uint8_t *out = dst;
    while (done < n) {
        uint32_t idx = f->pos / cbytes;
        if (f->cur_cluster == 0 || idx < f->cur_index) {
            f->cur_cluster = f->first_cluster; f->cur_index = 0;
        }
        while (f->cur_index < idx) {
            uint32_t nx;
            int r = next_cluster(fs, f->cur_cluster, &nx);
            if (r < 0) return r;
            if (is_end(fs, nx)) return FAT_EIO;      /* chain shorter than file size */
            f->cur_cluster = nx; f->cur_index++;
        }
        uint32_t off = f->pos % cbytes;
        const uint8_t *b = load(fs, cluster_lba(fs, f->cur_cluster) + off / 512u);
        if (!b) return FAT_EIO;
        uint32_t so = off % 512u, take = 512u - so;
        if (take > n - done) take = n - done;
        memcpy(out + done, b + so, take);
        done += take; f->pos += take;
    }
    return (int)done;
}

typedef struct { fat_list_cb cb; void *ctx; } list_ctx;
static int list_visit(const dirent_t *e, void *c)
{
    list_ctx *l = c;
    if (!strcmp(e->name, ".") || !strcmp(e->name, "..")) return 0;
    l->cb(e->name, e->size, (e->attr & FAT_ATTR_DIR) != 0, l->ctx);
    return 0;
}

int fat_list(fat_fs_t *fs, const char *path, fat_list_cb cb, void *ctx)
{
    fat_file_t d;
    int r = fat_open(fs, path, &d);
    if (r < 0) return r;
    if (!d.is_dir) return FAT_ENOTDIR;
    list_ctx l = { cb, ctx };
    return dir_walk(fs, d.first_cluster, list_visit, &l);
}
