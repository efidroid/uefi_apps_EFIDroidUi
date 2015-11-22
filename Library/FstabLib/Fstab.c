#include <Library/BaseLib.h>
#include <Uefi/UefiBaseType.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/Fstab.h>
#include "FstabPriv.h"

struct fs_mgr_flag_values {
    char *key_loc;
    long long part_length;
    char *label;
    int partnum;
    int swap_prio;
    unsigned int zram_size;
    unsigned int zram_streams;
    char* esp;
};

struct flag_list {
    const char *name;
    unsigned flag;
};

static struct flag_list mount_flags[] = {
    { "defaults",   0 },
    { 0,            0 },
};

static struct flag_list fs_mgr_flags[] = {
    { "multiboot",   MF_MULTIBOOT },
    { "uefi",        MF_UEFI },
    { "nvvars",      MF_NVVARS },
    { "esp=",        MF_ESP },
    { "defaults",    0 },
    { 0,             0 },
};

static int isspace(int c)
{
    return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
}

static char* strdup(const char *str)
{
    UINTN len;
    char *copy;

    len = AsciiStrLen(str) + 1;
    copy = AllocatePool(len);
    if (copy == NULL)
        return NULL;
    CopyMem(copy, str, len);
    return copy;
}

static
char *
strtok_r(char *s, const char *delim, char **last)
{
    char *spanp;
    int c, sc;
    char *tok;


    if (s == NULL && (s = *last) == NULL)
        return (NULL);

    /*
    * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
    */
    cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
        if (c == sc)
            goto cont;
    }

    if (c == 0) {		/* no non-delimiter characters */
        *last = NULL;
        return (NULL);
    }
    tok = s - 1;

    /*
    * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
    * Note that delim must have one NUL; we stop if we see that, too.
    */
    for (;;) {
        c = *s++;
        spanp = (char *)delim;
        do {
            if ((sc = *spanp++) == c) {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *last = s;
                return (tok);
            }
        } while (sc != 0);
    }
    /* NOTREACHED */
}


static INT32 getline(char**lineptr, UINTN *n, CONST CHAR8* data, UINTN size, UINTN* pos) {
    UINTN i;
    UINTN linesize = 0;

    // count bytes until a endline char
    for(i=*pos; i<size; i++) {
        linesize++;

        if(data[i]=='\n')
            break;
    }

    // EOF
    if(linesize==0)
        return -1;

    // allocate buffer
    *lineptr = ReallocatePool(*n, linesize+1, *lineptr);
    if(!(*lineptr))
        return -1;
    *n = linesize+1;

    // copy line
    CopyMem(*lineptr, &data[*pos], linesize);
    *lineptr[linesize] = 0;

    // update position
    (*pos) += linesize;

    return linesize;
}

static char *
strchr(const char *s, int c)
{
    for(; *s != (char) c; ++s)
        if (*s == '\0')
            return NULL;
    return (char *) s;
}

static void free(void *ptr) {
    if(0) FreePool(ptr);
}

#define ISPATHSEPARATOR(x) ((x == '/') || (x == '\\'))

#ifndef PATH_MAX
  #define PATH_MAX 5000
#endif

static char *
basename(char *path)
{
  static char singledot[] = ".";
  static char result[PATH_MAX];
  char *p, *lastp;
  UINTN len;

  /*
   * If `path' is a null pointer or points to an empty string,
   * return a pointer to the string ".".
   */
  if ((path == NULL) || (*path == '\0'))
    return (singledot);

  /* Strip trailing slashes, if any. */
  lastp = path + AsciiStrLen(path) - 1;
  while (lastp != path && ISPATHSEPARATOR(*lastp))
    lastp--;

  /* Now find the beginning of this (final) component. */
  p = lastp;
  while (p != path && !ISPATHSEPARATOR(*(p - 1)))
    p--;

  /* ...and copy the result into the result buffer. */
  len = (lastp - p) + 1 /* last char */;
  if (len > (PATH_MAX - 1))
    len = PATH_MAX - 1;

  CopyMem(result, p, len);
  result[len] = '\0';

  return (result);
}

char* util_basename(const char* path) {
    // duplicate input path
    char* str = strdup(path);
    if(!str) return NULL;

    // get basename
    char* bname = basename(str);
    if(!bname) {
        free(str);
        return NULL;
    }

    // duplicate return value
    char* ret = strdup(bname);

    // cleanup input path
    free(str);

    // return result
    return ret;
}

static int parse_flags(char *flags, struct flag_list *fl,
                       struct fs_mgr_flag_values *flag_vals,
                       char *fs_options, int fs_options_len)
{
    int f = 0;
    int i;
    char *p;
    char *savep;

    /* initialize flag values.  If we find a relevant flag, we'll
     * update the value */
    if (flag_vals) {
        SetMem(flag_vals, sizeof(*flag_vals), 0);
        flag_vals->partnum = -1;
        flag_vals->swap_prio = -1; /* negative means it wasn't specified. */
        flag_vals->zram_streams = 1;
    }

    /* initialize fs_options to the null string */
    if (fs_options && (fs_options_len > 0)) {
        fs_options[0] = '\0';
    }

    p = strtok_r(flags, ",", &savep);
    while (p) {
        /* Look for the flag "p" in the flag list "fl"
         * If not found, the loop exits with fl[i].name being null.
         */
        for (i = 0; fl[i].name; i++) {
            if (!AsciiStrnCmp(p, fl[i].name, AsciiStrLen(fl[i].name))) {
                f |= fl[i].flag;
                if ((fl[i].flag == MF_ESP) && flag_vals) {
                    flag_vals->esp = strdup(strchr(p, '=') + 1);
                }
                break;
            }
        }

        if (!fl[i].name) {
            if (fs_options) {
                /* It's not a known flag, so it must be a filesystem specific
                 * option.  Add it to fs_options if it was passed in.
                 */
                AsciiStrnCat(fs_options, p, fs_options_len);
                AsciiStrnCat(fs_options, ",", fs_options_len);
            } else {
                /* fs_options was not passed in, so if the flag is unknown
                 * it's an error.
                 */
                DEBUG((EFI_D_ERROR, "Warning: unknown flag %a\n", p));
            }
        }
        p = strtok_r(NULL, ",", &savep);
    }

    if (fs_options && fs_options[0]) {
        /* remove the last trailing comma from the list of options */
        fs_options[AsciiStrLen(fs_options) - 1] = '\0';
    }

    return f;
}

struct fstab *FstabParse(CONST CHAR8 *data, UINTN datasize)
{
    int cnt, entries;
    INT32 len;
    UINTN alloc_len = 0;
    char *line = NULL;
    const char *delim = " \t";
    char *save_ptr, *p;
    struct fstab *fstab = NULL;
    struct fs_mgr_flag_values flag_vals;
#define FS_OPTIONS_LEN 1024
    char* tmp_fs_options = AllocateZeroPool(FS_OPTIONS_LEN);
    UINTN BufferPosition = 0;

    entries = 0;
    while ((len = getline(&line, &alloc_len, data, datasize, &BufferPosition)) != -1) {
        /* if the last character is a newline, shorten the string by 1 byte */
        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        /* Skip any leading whitespace */
        p = line;
        while (isspace(*p)) {
            p++;
        }
        /* ignore comments or empty lines */
        if (*p == '#' || *p == '\0')
            continue;
        entries++;
    }

    if (!entries) {
        DEBUG((EFI_D_ERROR, "No entries found in fstab\n"));
        goto err;
    }

    /* Allocate and init the fstab structure */
    fstab = AllocateZeroPool(sizeof(struct fstab));
    fstab->num_entries = entries;
    //fstab->fstab_filename = strdup(fstab_path);
    fstab->recs = AllocateZeroPool(fstab->num_entries*sizeof(struct fstab_rec));

    BufferPosition = 0;

    cnt = 0;
    while ((len = getline(&line, &alloc_len, data, datasize, &BufferPosition)) != -1) {
        /* if the last character is a newline, shorten the string by 1 byte */
        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        /* Skip any leading whitespace */
        p = line;
        while (isspace(*p)) {
            p++;
        }
        /* ignore comments or empty lines */
        if (*p == '#' || *p == '\0')
            continue;

        /* If a non-comment entry is greater than the size we allocated, give an
         * error and quit.  This can happen in the unlikely case the file changes
         * between the two reads.
         */
        if (cnt >= entries) {
            DEBUG((EFI_D_ERROR, "Tried to process more entries than counted\n"));
            break;
        }

        if (!(p = strtok_r(line, delim, &save_ptr))) {
            DEBUG((EFI_D_ERROR, "Error parsing mount source\n"));
            goto err;
        }
        fstab->recs[cnt].blk_device = strdup(p);

        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            DEBUG((EFI_D_ERROR, "Error parsing mount_point\n"));
            goto err;
        }
        fstab->recs[cnt].mount_point = strdup(p);

        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            DEBUG((EFI_D_ERROR, "Error parsing fs_type\n"));
            goto err;
        }
        fstab->recs[cnt].fs_type = strdup(p);

        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            DEBUG((EFI_D_ERROR, "Error parsing mount_flags\n"));
            goto err;
        }
        tmp_fs_options[0] = '\0';
        fstab->recs[cnt].flags = parse_flags(p, mount_flags, NULL,
                                       tmp_fs_options, FS_OPTIONS_LEN);

        /* fs_options are optional */
        if (tmp_fs_options[0]) {
            fstab->recs[cnt].fs_options = strdup(tmp_fs_options);
        } else {
            fstab->recs[cnt].fs_options = NULL;
        }

        if (!(p = strtok_r(NULL, delim, &save_ptr))) {
            DEBUG((EFI_D_ERROR, "Error parsing fs_mgr_options\n"));
            goto err;
        }
        fstab->recs[cnt].fs_mgr_flags = parse_flags(p, fs_mgr_flags,
                                                    &flag_vals, NULL, 0);
        fstab->recs[cnt].key_loc = flag_vals.key_loc;
        fstab->recs[cnt].length = flag_vals.part_length;
        fstab->recs[cnt].label = flag_vals.label;
        fstab->recs[cnt].partnum = flag_vals.partnum;
        fstab->recs[cnt].swap_prio = flag_vals.swap_prio;
        fstab->recs[cnt].zram_size = flag_vals.zram_size;
        fstab->recs[cnt].zram_streams = flag_vals.zram_streams;
        fstab->recs[cnt].esp = flag_vals.esp;
        cnt++;
    }
    free(line);
    return fstab;

err:
    free(line);
    if (fstab)
        FsTabFree(fstab);
    return NULL;
}

void FsTabFree(struct fstab *fstab)
{
    int i;

    if (!fstab) {
        return;
    }

    for (i = 0; i < fstab->num_entries; i++) {
        /* Free the pointers return by strdup(3) */
        free(fstab->recs[i].blk_device);
        free(fstab->recs[i].mount_point);
        free(fstab->recs[i].fs_type);
        free(fstab->recs[i].fs_options);
        free(fstab->recs[i].key_loc);
        free(fstab->recs[i].label);
    }

    /* Free the fstab_recs array created by calloc(3) */
    free(fstab->recs);

    /* Free the fstab filename */
    free(fstab->fstab_filename);

    /* Free fstab */
    free(fstab);
}

int FstabIsMultiboot(struct fstab_rec *fstab)
{
    return fstab->fs_mgr_flags & (MF_MULTIBOOT);
}

int FstabIsUEFI(struct fstab_rec *fstab)
{
    return fstab->fs_mgr_flags & (MF_UEFI);
}

int FstabIsNVVARS(struct fstab_rec *fstab)
{
    return fstab->fs_mgr_flags & (MF_UEFI);
}

FSTAB_REC* FstabGetESP(struct fstab *fstab)
{
    int i;

    for(i=0; i<fstab->num_entries; i++) {
        if(fstab->recs[i].esp)
            return &fstab->recs[i];
    }

    return NULL;
}

CHAR8* FstabGetPartitionName(FSTAB_REC* Rec) {
    if (AsciiStrStr(Rec->blk_device, "by-name") != NULL) {
		CHAR8* name = util_basename(Rec->blk_device);
        return name;
	}

    return NULL;
}

FSTAB_REC* FstabGetByPartitionName(FSTAB *fstab, CONST CHAR8* SearchName) {
    int i;

    if(!fstab)
        return NULL;

    for(i=0; i<fstab->num_entries; i++) {
        FSTAB_REC* Rec = &fstab->recs[i];

        CHAR8* Name = FstabGetPartitionName(Rec);
        if(!Name) continue;

        if(!AsciiStrCmp(Name, SearchName)) {
            FreePool(Name);
            return Rec;
        }

        FreePool(Name);
    }

    return NULL;
}
