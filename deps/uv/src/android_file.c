#include "android_file.h"
#include "murmur3.h"

typedef struct AndroidAssetStruct {
  int fd;            /* we'll use this field as the key */
  AAsset *asset;
  AAssetDir *assetDir;
  UT_hash_handle hh; /* makes this structure hashable */
} AndroidAsset;
AndroidAsset *androidAssets = NULL;

AAssetManager *android_asset_manager = NULL;

typedef struct AssetStatStruct {
  char name[256];
  uint32_t key;
  uint32_t parentKey;
  size_t type;
  size_t size;
} AssetStat;
AssetStat *assetStats = NULL;
size_t numAssetStats = 0;

const uint32_t FILE_TYPE = 1;
const uint32_t DIRECTORY_TYPE = 2;

/* __attribute__ ((dllexport)) */ void initAssetManager(AAssetManager *am) {
  android_asset_manager = am;
  // assetStats = as;
  // numAssetStats = nas;
  AAsset *asset = AAssetManager_open(android_asset_manager, "index.bin", AASSET_MODE_UNKNOWN);
  size_t size = AAsset_getLength64(asset);
  char *buf = malloc(size);
  int result = AAsset_read(asset, buf, size);
  if (result > 0) {
    assetStats = (AssetStat *)buf;
    numAssetStats = size/sizeof(AssetStat);

    {
      AssetStat *assetStat = &assetStats[0];
      printf("got asset 0 '%s' %x %x %lx", assetStat->name, assetStat->key, assetStat->parentKey, assetStat->size); fflush(stdout);
    }
    {
      AssetStat *assetStat = &assetStats[1];
      printf("got asset 1 '%s' %x %x %lx ", assetStat->name, assetStat->key, assetStat->parentKey, assetStat->size); fflush(stdout);
    }
  } else {
    printf("failed to read asset index\n");
    fflush(stdout);
    abort();
  }
  AAsset_close(asset);
}
uint32_t getKey(const char *path) {
  /* int len = strlen(path);
  if (len > 0) { */
    uint32_t key;
    MurmurHash3_x86_32(path, strlen(path), 0, &key);
    return key;
  /* } else {
    return 0xaf56fc23; // murmur3("");
  } */
}
AssetStat *getAssetStat(const char *path) {
  uint32_t key = getKey(path);
  for (size_t i = 0; i < numAssetStats; i++) {
    AssetStat *assetStat = &assetStats[i];
    if (assetStat->key == key) {
      return assetStat;
    }
  }
  return NULL;
}

int androidAssetId = 0x40000000; // halfway to negative
const uint64_t blockSize = 4096;

int startsWith(const char *pre, const char *str) {
  return strncmp(pre, str, strlen(pre)) == 0;
}
int isPackagePath(const char *p) {
  return startsWith("/package", p);
}
const char *getPackageSubpath(const char *p) {
  p = p + (sizeof("/package")-1);
  if (p[0] == '/') {
    p++;
  }
  return p;
}

int android_access(const char *pathname, int mode) {
  if (isPackagePath(pathname)) {
    const char *subpath = getPackageSubpath(pathname);
    AssetStat *assetStat = getAssetStat(subpath);
    if (assetStat) {
      return 0;
    } else {
      errno = EACCES;
      return -1;
    }
  } else {
    return access(pathname, mode);
  }
}

int android_chmod(const char *pathname, mode_t mode) {
  if (isPackagePath(pathname)) {
    errno = EACCES;
    return -1;
  } else {
    return chmod(pathname, mode);
  }
}

int android_chown(const char *pathname, uid_t owner, gid_t group) {
  if (isPackagePath(pathname)) {
    errno = EACCES;
    return -1;
  } else {
    return chown(pathname, owner, group);
  }
}

int android_close(int fd) {
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);

  if (iter) {
    AAsset *asset = iter->asset;
    AAsset_close(asset);
    HASH_DEL(androidAssets, iter);
    return 0;
  } else {
    return close(fd);
  }
}

ssize_t android_copyfile(uv_fs_t* req) {
  const char *pathname = req->path;
  if (isPackagePath(pathname)) {
    errno = ENOTSUP;
    return -1;
  } else {
    return uv__fs_copyfile(req);
  }
}

int android_fchmod(int fd, mode_t mode) {
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    errno = EACCES;
    return -1;
  } else {
    return fchmod(fd, mode);
  }
}

int android_fchown(int fd, uid_t owner, gid_t group) {
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    errno = EACCES;
    return -1;
  } else {
    return fchown(fd, owner, group);
  }
}

int android_lchown(const char *path, uid_t owner, gid_t group) {
  if (isPackagePath(path)) {
    errno = EACCES;
    return -1;
  } else {
    return lchown(path, owner, group);
  }
}

ssize_t android_fdatasync(uv_fs_t* req) {
  int fd = req->file;
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    errno = EACCES;
    return -1;
  } else {
    return uv__fs_fdatasync(req);
  }
}

void setFileStat(uv_stat_t *buf, uint32_t key, size_t size) {
  memset(buf, 0, sizeof(*buf));

  // buf->st_dev = 0;
  buf->st_dev = 1;
  buf->st_mode = S_IFREG;
  buf->st_nlink = 1;
  buf->st_uid = 0;
  buf->st_gid = 0;
  // buf->st_rdev = 0;
  buf->st_ino = key;
  buf->st_size = size;
  buf->st_blksize = blockSize;
  buf->st_blocks = size/512;
  // buf->st_flags = 0;
  // buf->st_gen = 0;
  // buf->st_atim = uv_timespec_t{0,0};
  // buf->st_mtim = uv_timespec_t{0,0};
  // buf->st_ctim = uv_timespec_t{0,0};
  // buf->st_birthtim = uv_timespec_t{0,0};
}
void setDirStat(uv_stat_t *buf, uint32_t key) {
  memset(buf, 0, sizeof(*buf));

  // buf->st_dev = 0;
  buf->st_dev = 1;
  buf->st_mode = S_IFDIR;
  buf->st_nlink = 1;
  buf->st_uid = 0;
  buf->st_gid = 0;
  // buf->st_rdev = 0;
  buf->st_ino = key;
  buf->st_size = blockSize;
  buf->st_blksize = blockSize;
  buf->st_blocks = 1;
  // buf->st_flags = 0;
  // buf->st_gen = 0;
  // buf->st_atim = uv_timespec_t{0,0};
  // buf->st_mtim = uv_timespec_t{0,0};
  // buf->st_ctim = uv_timespec_t{0,0};
  // buf->st_birthtim = uv_timespec_t{0,0};
}
int android_fstat(int fd, uv_stat_t *buf) {
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    AAsset *asset = iter->asset;
    if (asset) {
      size_t size = AAsset_getLength64(asset);

      setFileStat(buf, fd, size);

      return 0;
    } else {
      setDirStat(buf, fd);

      return 0;
    }
  } else {
    return uv__fs_fstat(fd, buf);
  }
}

ssize_t android_fsync(uv_fs_t* req) {
  int fd = req->file;
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    errno = EACCES;
    return -1;
  } else {
    return uv__fs_fsync(req);
  }
}

int android_ftruncate(int fd, off_t length) {
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    errno = EACCES;
    return -1;
  } else {
    return ftruncate(fd, length);
  }
}

ssize_t android_futime(uv_fs_t* req) {
  int fd = req->file;
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    errno = EACCES;
    return -1;
  } else {
    return uv__fs_futime(req);
  }
}

int android_lstat(const char *path, uv_stat_t *buf) {
  // uint64_t out[2];
  // MurmurHash3_x64_128(path, strlen(path), 0, out);
  if (isPackagePath(path)) {
    const char *subpath = getPackageSubpath(path);

    if (subpath[0] != '\0') {
      AssetStat *assetStat = getAssetStat(subpath);
      if (assetStat) {
        if (assetStat->type == FILE_TYPE) {
          uint64_t size = assetStat->size;

          setFileStat(buf, assetStat->key, size);
        
          return 0;
        } else if (assetStat->type == DIRECTORY_TYPE) {
          setDirStat(buf, assetStat->key);

          return 0;
        }
      }

      errno = ENOENT;

      return -1;
    } else {
      setDirStat(buf, 1);

      return 0;
    }
  } else {
    return uv__fs_lstat(path, buf);
  }
}

int android_link(const char *oldpath, const char *newpath) {
  if (isPackagePath(oldpath) || isPackagePath(newpath)) {
    errno = EACCES;
    return -1;
  } else {
    return link(oldpath, newpath);
  }
}

int android_mkdir(const char *pathname, mode_t mode) {
  if (isPackagePath(pathname)) {
    errno = EACCES;
    return -1;
  } else {
    return mkdir(pathname, mode);
  }
}

ssize_t android_mkdtemp(uv_fs_t* req) {
  return uv__fs_mkdtemp(req);
}

ssize_t android_open(uv_fs_t* req) {
  const char *pathname = req->path;
  if (isPackagePath(pathname)) {
    const char *subpath = getPackageSubpath(pathname);
    AssetStat *assetStat = getAssetStat(subpath);
    
    if (assetStat) {
      if (assetStat->type == FILE_TYPE) {
        AAsset* asset = AAssetManager_open(android_asset_manager, subpath, AASSET_MODE_UNKNOWN);

        if (asset) {
          AndroidAsset *iter = malloc(sizeof(AndroidAsset));
          iter->fd = androidAssetId++;
          iter->asset = asset;
          iter->assetDir = NULL;
          HASH_ADD_INT(androidAssets, fd, iter);

          return iter->fd;
        }
      } else if (assetStat->type == DIRECTORY_TYPE) {
        AAssetDir* assetDir = AAssetManager_openDir(android_asset_manager, subpath);

        AndroidAsset *iter = malloc(sizeof(AndroidAsset));
        iter->fd = androidAssetId++;
        iter->assetDir = assetDir;
        iter->asset = NULL;
        HASH_ADD_INT(androidAssets, fd, iter);

        return iter->fd;
      }
    }

    errno = ENOENT;
    return -1;
  } else {
    return uv__fs_open(req);
  }
}

ssize_t android_read(uv_fs_t* req) {
  int fd = req->file;
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    AAsset *asset = iter->asset;

    unsigned int iovmax = uv__getiovmax();
    if (req->nbufs > iovmax) {
      req->nbufs = iovmax;
    }

    if (req->off >= 0) {
      AAsset_seek(asset, req->off, SEEK_SET);
    }

    int result = 0;
    for (unsigned int i = 0; i < req->nbufs; i++) {
      int readResult = AAsset_read(asset, req->bufs[i].base, req->bufs[i].len);
      if (readResult > 0) {
        result += readResult;
      } else {
        return readResult;
      }
    }
    return result;
  } else {
    return uv__fs_read(req);
  }
}

int android_fs_scandir_sort(uv__dirent_t* a, uv__dirent_t* b) {
  return strcmp(a->d_name, b->d_name);
}
ssize_t android_scandir(uv_fs_t* req) {
  const char *pathname = req->path;
  if (isPackagePath(pathname)) {
    const char *subpath = getPackageSubpath(pathname);

    uv__dirent_t dirents[4096];
    ssize_t numDirents = 0;
    
    uint32_t parentKey = getKey(subpath);
    for (size_t i = 0; i < numAssetStats; i++) {
      AssetStat *assetStat = &assetStats[i];
      
      if (assetStat->parentKey == parentKey) {
        uv__dirent_t dirent;
        dirent.d_type = assetStat->type == FILE_TYPE ? UV__DT_FILE : UV__DT_DIR;
        dirent.d_ino = assetStat->key;
        dirent.d_off = i;
        dirent.d_reclen = sizeof(dirent);
        strncpy(dirent.d_name, assetStat->name, sizeof(dirent.d_name));
        
        printf("scandir 2 '%s' '%s', %x %lx\n", assetStat->name, dirent.d_name, dirent.d_ino, sizeof(dirent.d_name)); fflush(stdout);

        memcpy(&dirents[numDirents++], &dirent, sizeof(dirent));
        // dirents[numDirents++] = dirent;
      }
    }

    if (numDirents > 0) {
      uv__dirent_t **dirents2 = malloc(i*sizeof(uv__dirent_t*));
      for (int j = 0; j < i; j++) {
        dirents2[j] = malloc(sizeof(uv__dirent_t));
        memcpy(dirents2[j], &dirents[j], sizeof(uv__dirent_t));
      }

      req->ptr = dirents2;
      req->nbufs = 0;
    }
    return numDirents;
  } else {
    return uv__fs_scandir(req);
  }
}

ssize_t android_readlink(uv_fs_t* req) {
  const char *pathname = req->path;
  if (isPackagePath(pathname)) {
    const char *subpath = getPackageSubpath(pathname);
    AssetStat *assetStat = getAssetStat(subpath);
    if (assetStat) {
      errno = EINVAL;
      return -1;
    } else {
      errno = ENOENT;
      return -1;
    }
  } else {
    return uv__fs_readlink(req);
  }
}

ssize_t android_realpath(uv_fs_t* req) {
  const char *pathname = req->path;
  if (isPackagePath(pathname)) {
    ssize_t len = uv__fs_pathmax_size(req->path);
    char* buf = (char *)uv__malloc(len + 1);
    memcpy(buf, req->path, len + 1);

    req->ptr = buf;

    return 0;
  } else {
    return uv__fs_realpath(req);
  }
}

int android_rename(const char *oldpath, const char *newpath) {
  if (isPackagePath(oldpath) || isPackagePath(newpath)) {
    errno = EACCES;
    return -1;
  } else {
    return rename(oldpath, newpath);
  }
}

int android_rmdir(const char *path) {
  if (isPackagePath(path)) {
    errno = EACCES;
    return -1;
  } else {
    return rmdir(path);
  }
}

ssize_t android_sendfile(uv_fs_t* req) {
  int in_fd = req->flags;
  int out_fd = req->file;

  AndroidAsset *iter1;
  HASH_FIND_INT(androidAssets, &out_fd, iter1);
  AndroidAsset *iter2;
  HASH_FIND_INT(androidAssets, &in_fd, iter2);
  if (iter1 || iter2) {
    errno = EACCES;
    return -1;
  } else {
    return uv__fs_sendfile(req);
  }
}

int android_stat(const char *path, uv_stat_t *buf) {
  if (isPackagePath(path)) {
    return android_lstat(path, buf);
  } else {
    return uv__fs_stat(path, buf);
  }
}

int android_symlink(const char *target, const char *linkpath) {
  if (isPackagePath(target) || isPackagePath(linkpath)) {
    errno = EACCES;
    return -1;
  } else {
    return symlink(target, linkpath);
  }
}

int android_unlink(const char *pathname) {
  if (isPackagePath(pathname)) {
    errno = EACCES;
    return -1;
  } else {
    return unlink(pathname);
  }
}

ssize_t android_utime(uv_fs_t* req) {
  const char *pathname = req->path;
  if (isPackagePath(pathname)) {
    errno = EACCES;
    return -1;
  } else {
    return uv__fs_utime(req);
  }
}

ssize_t android_write_all(uv_fs_t* req) {
  int fd = req->file;
  AndroidAsset *iter;
  HASH_FIND_INT(androidAssets, &fd, iter);
  if (iter) {
    errno = EACCES;
    return -1;
  } else {
    return uv__fs_write_all(req);
  }
}
