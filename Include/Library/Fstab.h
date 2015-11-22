#ifndef FSTAB_H
#define FSTAB_H 1

/*
 * The entries must be kept in the same order as they were seen in the fstab.
 * Unless explicitly requested, a lookup on mount point should always
 * return the 1st one.
 */
struct fstab {
    int num_entries;
    struct fstab_rec *recs;
    char *fstab_filename;
};

struct fstab_rec {
    char *blk_device;
    char *mount_point;
    char *fs_type;
    unsigned long flags;
    char *fs_options;
    int fs_mgr_flags;
    char *key_loc;
    char *verity_loc;
    long long length;
    char *label;
    int partnum;
    int swap_prio;
    unsigned int zram_size;
    unsigned int zram_streams;
    char* esp;
};

typedef struct fstab FSTAB;
typedef struct fstab_rec FSTAB_REC;

struct fstab *FstabParse(CONST CHAR8 *data, UINTN datasize);
void FsTabFree(struct fstab *fstab);

int FstabIsMultiboot(struct fstab_rec *fstab);
int FstabIsUEFI(struct fstab_rec *fstab);
int FstabIsNVVARS(struct fstab_rec *fstab);
FSTAB_REC* FstabGetESP(struct fstab *fstab);
CHAR8* FstabGetPartitionName(FSTAB_REC* Rec);
FSTAB_REC* FstabGetByPartitionName(FSTAB *fstab, CONST CHAR8* SearchName);

#endif /* ! FSTAB_H */
