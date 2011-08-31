/*
 * libeio API header
 *
 * Copyright (c) 2007,2008,2009,2010,2011 Marc Alexander Lehmann <libeio@schmorp.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modifica-
 * tion, are permitted provided that the following conditions are met:
 * 
 *   1.  Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 * 
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MER-
 * CHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPE-
 * CIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTH-
 * ERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License ("GPL") version 2 or any later version,
 * in which case the provisions of the GPL are applicable instead of
 * the above. If you wish to allow the use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the BSD license, indicate your decision
 * by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL. If you do not delete the
 * provisions above, a recipient may use your version of this file under
 * either the BSD or the GPL.
 */

#ifndef EIO_H_
#define EIO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <signal.h>
#include <sys/types.h>

typedef struct eio_req    eio_req;
typedef struct eio_dirent eio_dirent;

typedef int (*eio_cb)(eio_req *req);

#ifndef EIO_REQ_MEMBERS
# define EIO_REQ_MEMBERS
#endif

#ifndef EIO_STRUCT_STAT
# ifdef _WIN32
#  define EIO_STRUCT_STAT struct _stati64
#  define EIO_STRUCT_STATI64
# else
#  define EIO_STRUCT_STAT struct stat
# endif
#endif

#ifdef _WIN32
  typedef int      eio_uid_t;
  typedef int      eio_gid_t;
  typedef int      eio_mode_t;
  #ifdef __MINGW32__ /* no intptr_t */
    typedef ssize_t  eio_ssize_t;
  #else
    typedef intptr_t eio_ssize_t; /* or SSIZE_T */
  #endif
  #if __GNUC__
    typedef long long eio_ino_t;
  #else
    typedef __int64   eio_ino_t; /* unsigned not supported by msvc */
  #endif
#else
  typedef uid_t    eio_uid_t;
  typedef gid_t    eio_gid_t;
  typedef ssize_t  eio_ssize_t;
  typedef ino_t    eio_ino_t;
  typedef mode_t   eio_mode_t;
#endif

#ifndef EIO_STRUCT_STATVFS
# define EIO_STRUCT_STATVFS struct statvfs
#endif

/* for readdir */

/* eio_readdir flags */
enum
{
  EIO_READDIR_DENTS         = 0x01, /* ptr2 contains eio_dirents, not just the (unsorted) names */
  EIO_READDIR_DIRS_FIRST    = 0x02, /* dirents gets sorted into a good stat() ing order to find directories first */
  EIO_READDIR_STAT_ORDER    = 0x04, /* dirents gets sorted into a good stat() ing order to quickly stat all files */
  EIO_READDIR_FOUND_UNKNOWN = 0x80, /* set by eio_readdir when *_ARRAY was set and any TYPE=UNKNOWN's were found */

  EIO_READDIR_CUSTOM1       = 0x100, /* for use by apps */
  EIO_READDIR_CUSTOM2       = 0x200  /* for use by apps */
};

/* using "typical" values in the hope that the compiler will do something sensible */
enum eio_dtype
{
  EIO_DT_UNKNOWN =  0,
  EIO_DT_FIFO    =  1,
  EIO_DT_CHR     =  2,
  EIO_DT_MPC     =  3, /* multiplexed char device (v7+coherent) */
  EIO_DT_DIR     =  4,
  EIO_DT_NAM     =  5, /* xenix special named file */
  EIO_DT_BLK     =  6,
  EIO_DT_MPB     =  7, /* multiplexed block device (v7+coherent) */
  EIO_DT_REG     =  8,
  EIO_DT_NWK     =  9, /* HP-UX network special */
  EIO_DT_CMP     =  9, /* VxFS compressed */
  EIO_DT_LNK     = 10,
  /*  DT_SHAD    = 11,*/
  EIO_DT_SOCK    = 12,
  EIO_DT_DOOR    = 13, /* solaris door */
  EIO_DT_WHT     = 14,
  EIO_DT_MAX     = 15  /* highest DT_VALUE ever, hopefully */
};

struct eio_dirent
{
  int nameofs; /* offset of null-terminated name string in (char *)req->ptr2 */
  unsigned short namelen; /* size of filename without trailing 0 */
  unsigned char type; /* one of EIO_DT_* */
  signed char score; /* internal use */
  eio_ino_t inode; /* the inode number, if available, otherwise unspecified */
};

/* eio_msync flags */
enum
{
  EIO_MS_ASYNC      = 1,
  EIO_MS_INVALIDATE = 2,
  EIO_MS_SYNC       = 4
};

/* eio_mtouch flags */
enum
{
  EIO_MT_MODIFY     = 1
};

/* eio_sync_file_range flags */
enum
{
  EIO_SYNC_FILE_RANGE_WAIT_BEFORE = 1,
  EIO_SYNC_FILE_RANGE_WRITE       = 2,
  EIO_SYNC_FILE_RANGE_WAIT_AFTER  = 4
};

/* eio_fallocate flags */
enum
{
  EIO_FALLOC_FL_KEEP_SIZE = 1 /* MUST match the value in linux/falloc.h */
};

/* timestamps and differences - feel free to use double in your code directly */
typedef double eio_tstamp;

/* the eio request structure */
enum
{
  EIO_CUSTOM,
  EIO_OPEN, EIO_CLOSE, EIO_DUP2,
  EIO_READ, EIO_WRITE,
  EIO_READAHEAD, EIO_SENDFILE,
  EIO_STAT, EIO_LSTAT, EIO_FSTAT,
  EIO_STATVFS, EIO_FSTATVFS,
  EIO_TRUNCATE, EIO_FTRUNCATE,
  EIO_UTIME, EIO_FUTIME,
  EIO_CHMOD, EIO_FCHMOD,
  EIO_CHOWN, EIO_FCHOWN,
  EIO_SYNC, EIO_FSYNC, EIO_FDATASYNC, EIO_SYNCFS,
  EIO_MSYNC, EIO_MTOUCH, EIO_SYNC_FILE_RANGE, EIO_FALLOCATE,
  EIO_MLOCK, EIO_MLOCKALL,
  EIO_UNLINK, EIO_RMDIR, EIO_MKDIR, EIO_RENAME,
  EIO_MKNOD, EIO_READDIR,
  EIO_LINK, EIO_SYMLINK, EIO_READLINK, EIO_REALPATH,
  EIO_GROUP, EIO_NOP,
  EIO_BUSY
};

/* mlockall constants */
enum
{
  EIO_MCL_CURRENT = 1,
  EIO_MCL_FUTURE  = 2
};

/* request priorities */

enum {
  EIO_PRI_MIN     = -4,
  EIO_PRI_MAX     =  4,
  EIO_PRI_DEFAULT =  0
};

/* eio request structure */
/* this structure is mostly read-only */
/* when initialising it, all members must be zero-initialised */
struct eio_req
{
  eio_req volatile *next; /* private ETP */

  eio_ssize_t result;  /* result of syscall, e.g. result = read (... */
  off_t offs;      /* read, write, truncate, readahead, sync_file_range, fallocate: file offset, mknod: dev_t */
  size_t size;     /* read, write, readahead, sendfile, msync, mlock, sync_file_range, fallocate: length */
  void *ptr1;      /* all applicable requests: pathname, old name; readdir: optional eio_dirents */
  void *ptr2;      /* all applicable requests: new name or memory buffer; readdir: name strings */
  eio_tstamp nv1;  /* utime, futime: atime; busy: sleep time */
  eio_tstamp nv2;  /* utime, futime: mtime */

  int type;        /* EIO_xxx constant ETP */
  int int1;        /* all applicable requests: file descriptor; sendfile: output fd; open, msync, mlockall, readdir: flags */
  long int2;       /* chown, fchown: uid; sendfile: input fd; open, chmod, mkdir, mknod: file mode, sync_file_range, fallocate: flags */
  long int3;       /* chown, fchown: gid */
  int errorno;     /* errno value on syscall return */

#if __i386 || __amd64
  unsigned char cancelled;
#else
  sig_atomic_t cancelled;
#endif

  unsigned char flags; /* private */
  signed char pri;     /* the priority */

  void *data;
  eio_cb finish;
  void (*destroy)(eio_req *req); /* called when request no longer needed */
  void (*feed)(eio_req *req);    /* only used for group requests */

  EIO_REQ_MEMBERS

  eio_req *grp, *grp_prev, *grp_next, *grp_first; /* private */
};

/* _private_ request flags */
enum {
  EIO_FLAG_PTR1_FREE = 0x01, /* need to free(ptr1) */
  EIO_FLAG_PTR2_FREE = 0x02, /* need to free(ptr2) */
  EIO_FLAG_GROUPADD  = 0x04  /* some request was added to the group */
};

/* undocumented/unsupported/private helper */
/*void eio_page_align (void **addr, size_t *length);*/

/* returns < 0 on error, errno set
 * need_poll, if non-zero, will be called when results are available
 * and eio_poll_cb needs to be invoked (it MUST NOT call eio_poll_cb itself).
 * done_poll is called when the need to poll is gone.
 */
int eio_init (void (*want_poll)(void), void (*done_poll)(void));

/* must be called regularly to handle pending requests */
/* returns 0 if all requests were handled, -1 if not, or the value of EIO_FINISH if != 0 */
int eio_poll (void);

/* stop polling if poll took longer than duration seconds */
void eio_set_max_poll_time (eio_tstamp nseconds);
/* do not handle more then count requests in one call to eio_poll_cb */
void eio_set_max_poll_reqs (unsigned int nreqs);

/* set minimum required number
 * maximum wanted number
 * or maximum idle number of threads */
void eio_set_min_parallel (unsigned int nthreads);
void eio_set_max_parallel (unsigned int nthreads);
void eio_set_max_idle     (unsigned int nthreads);
void eio_set_idle_timeout (unsigned int seconds);

unsigned int eio_nreqs    (void); /* number of requests in-flight */
unsigned int eio_nready   (void); /* number of not-yet handled requests */
unsigned int eio_npending (void); /* number of finished but unhandled requests */
unsigned int eio_nthreads (void); /* number of worker threads in use currently */

/*****************************************************************************/
/* convenience wrappers */

#ifndef EIO_NO_WRAPPERS
eio_req *eio_nop       (int pri, eio_cb cb, void *data); /* does nothing except go through the whole process */
eio_req *eio_busy      (eio_tstamp delay, int pri, eio_cb cb, void *data); /* ties a thread for this long, simulating busyness */
eio_req *eio_sync      (int pri, eio_cb cb, void *data);
eio_req *eio_fsync     (int fd, int pri, eio_cb cb, void *data);
eio_req *eio_fdatasync (int fd, int pri, eio_cb cb, void *data);
eio_req *eio_syncfs    (int fd, int pri, eio_cb cb, void *data);
eio_req *eio_msync     (void *addr, size_t length, int flags, int pri, eio_cb cb, void *data);
eio_req *eio_mtouch    (void *addr, size_t length, int flags, int pri, eio_cb cb, void *data);
eio_req *eio_mlock     (void *addr, size_t length, int pri, eio_cb cb, void *data);
eio_req *eio_mlockall  (int flags, int pri, eio_cb cb, void *data);
eio_req *eio_sync_file_range (int fd, off_t offset, size_t nbytes, unsigned int flags, int pri, eio_cb cb, void *data);
eio_req *eio_fallocate (int fd, int mode, off_t offset, size_t len, int pri, eio_cb cb, void *data);
eio_req *eio_close     (int fd, int pri, eio_cb cb, void *data);
eio_req *eio_readahead (int fd, off_t offset, size_t length, int pri, eio_cb cb, void *data);
eio_req *eio_read      (int fd, void *buf, size_t length, off_t offset, int pri, eio_cb cb, void *data);
eio_req *eio_write     (int fd, void *buf, size_t length, off_t offset, int pri, eio_cb cb, void *data);
eio_req *eio_fstat     (int fd, int pri, eio_cb cb, void *data); /* stat buffer=ptr2 allocated dynamically */
eio_req *eio_fstatvfs  (int fd, int pri, eio_cb cb, void *data); /* stat buffer=ptr2 allocated dynamically */
eio_req *eio_futime    (int fd, eio_tstamp atime, eio_tstamp mtime, int pri, eio_cb cb, void *data);
eio_req *eio_ftruncate (int fd, off_t offset, int pri, eio_cb cb, void *data);
eio_req *eio_fchmod    (int fd, eio_mode_t mode, int pri, eio_cb cb, void *data);
eio_req *eio_fchown    (int fd, eio_uid_t uid, eio_gid_t gid, int pri, eio_cb cb, void *data);
eio_req *eio_dup2      (int fd, int fd2, int pri, eio_cb cb, void *data);
eio_req *eio_sendfile  (int out_fd, int in_fd, off_t in_offset, size_t length, int pri, eio_cb cb, void *data);
eio_req *eio_open      (const char *path, int flags, eio_mode_t mode, int pri, eio_cb cb, void *data);
eio_req *eio_utime     (const char *path, eio_tstamp atime, eio_tstamp mtime, int pri, eio_cb cb, void *data);
eio_req *eio_truncate  (const char *path, off_t offset, int pri, eio_cb cb, void *data);
eio_req *eio_chown     (const char *path, eio_uid_t uid, eio_gid_t gid, int pri, eio_cb cb, void *data);
eio_req *eio_chmod     (const char *path, eio_mode_t mode, int pri, eio_cb cb, void *data);
eio_req *eio_mkdir     (const char *path, eio_mode_t mode, int pri, eio_cb cb, void *data);
eio_req *eio_readdir   (const char *path, int flags, int pri, eio_cb cb, void *data); /* result=ptr2 allocated dynamically */
eio_req *eio_rmdir     (const char *path, int pri, eio_cb cb, void *data);
eio_req *eio_unlink    (const char *path, int pri, eio_cb cb, void *data);
eio_req *eio_readlink  (const char *path, int pri, eio_cb cb, void *data); /* result=ptr2 allocated dynamically */
eio_req *eio_realpath  (const char *path, int pri, eio_cb cb, void *data); /* result=ptr2 allocated dynamically */
eio_req *eio_stat      (const char *path, int pri, eio_cb cb, void *data); /* stat buffer=ptr2 allocated dynamically */
eio_req *eio_lstat     (const char *path, int pri, eio_cb cb, void *data); /* stat buffer=ptr2 allocated dynamically */
eio_req *eio_statvfs   (const char *path, int pri, eio_cb cb, void *data); /* stat buffer=ptr2 allocated dynamically */
eio_req *eio_mknod     (const char *path, eio_mode_t mode, dev_t dev, int pri, eio_cb cb, void *data);
eio_req *eio_link      (const char *path, const char *new_path, int pri, eio_cb cb, void *data);
eio_req *eio_symlink   (const char *path, const char *new_path, int pri, eio_cb cb, void *data);
eio_req *eio_rename    (const char *path, const char *new_path, int pri, eio_cb cb, void *data);
eio_req *eio_custom    (void (*execute)(eio_req *), int pri, eio_cb cb, void *data);
#endif

/*****************************************************************************/
/* groups */

eio_req *eio_grp       (eio_cb cb, void *data);
void eio_grp_feed      (eio_req *grp, void (*feed)(eio_req *req), int limit);
void eio_grp_limit     (eio_req *grp, int limit);
void eio_grp_add       (eio_req *grp, eio_req *req);
void eio_grp_cancel    (eio_req *grp); /* cancels all sub requests but not the group */

/*****************************************************************************/
/* request api */

/* true if the request was cancelled, useful in the invoke callback */
#define EIO_CANCELLED(req)   ((req)->cancelled)

#define EIO_RESULT(req)      ((req)->result)
/* returns a pointer to the result buffer allocated by eio */
#define EIO_BUF(req)         ((req)->ptr2)
#define EIO_STAT_BUF(req)    ((EIO_STRUCT_STAT    *)EIO_BUF(req))
#define EIO_STATVFS_BUF(req) ((EIO_STRUCT_STATVFS *)EIO_BUF(req))
#define EIO_PATH(req)        ((char *)(req)->ptr1)

/* submit a request for execution */
void eio_submit (eio_req *req);
/* cancel a request as soon fast as possible, if possible */
void eio_cancel (eio_req *req);

/*****************************************************************************/
/* convenience functions */

eio_ssize_t eio_sendfile_sync (int ofd, int ifd, off_t offset, size_t count);
eio_ssize_t eio__pread        (int fd, void *buf, size_t count, off_t offset);
eio_ssize_t eio__pwrite       (int fd, void *buf, size_t count, off_t offset);

#ifdef __cplusplus
}
#endif

#endif

