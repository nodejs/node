#include "node_sqlite_vfs.h"

#include "env-inl.h"
#include "path.h"
#include "permission/permission.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <string>
#include <string_view>

namespace node {
namespace sqlite {

namespace {

std::atomic<uint64_t> next_vfs_id{0};

bool IsSidecarPath(std::string_view path, std::string* database_path) {
  constexpr std::string_view suffixes[] = {"-journal", "-wal", "-shm"};
  for (const std::string_view suffix : suffixes) {
    if (path.size() > suffix.size() && path.ends_with(suffix)) {
      *database_path = path.substr(0, path.size() - suffix.size());
      return true;
    }
  }

  // SQLite names super-journals as <database>-mjXXXXXX9XX, where X is a hex
  // digit (see pagerIsSuperJrnlName()). A permission granted for the
  // database also covers its super-journal.
  constexpr size_t kSuperJournalSuffixLength = 12;
  if (path.size() > kSuperJournalSuffixLength) {
    const std::string_view suffix =
        path.substr(path.size() - kSuperJournalSuffixLength);
    if (suffix.starts_with("-mj") && suffix[9] == '9' &&
        std::all_of(suffix.begin() + 3, suffix.end(), [](char c) {
          return std::isxdigit(static_cast<unsigned char>(c));
        })) {
      *database_path = path.substr(0, path.size() - suffix.size());
      return true;
    }
  }

  return false;
}

sqlite3_filename CreateReadOnlyShmFilename(sqlite3_filename name) {
  const char* parameters[] = {"readonly_shm", "1"};
  return sqlite3_create_filename(sqlite3_filename_database(name),
                                 sqlite3_filename_journal(name),
                                 sqlite3_filename_wal(name),
                                 1,
                                 parameters);
}

std::string CanonicalPath(const SQLitePermissionVFS* vfs, const char* path) {
  // SQLite normally passes a full pathname to xOpen, but xAccess and xDelete
  // may receive names produced by SQLite itself. Canonicalize those names
  // before consulting Node's path permission tree.
  std::string result(static_cast<size_t>(vfs->parent()->mxPathname) + 1, '\0');
  if (vfs->FullPathname(
          path, static_cast<int>(result.size()), result.data()) != SQLITE_OK) {
    return {};
  }
  result.resize(strlen(result.c_str()));
  return result;
}

struct PermissionFile {
  sqlite3_file base;
  SQLitePermissionVFS* vfs;
  // Read-only SHM clone of the filename, set when sidecars are not writable.
  sqlite3_filename parent_name;
  bool can_write;
  bool can_write_sidecar;
  sqlite3_io_methods methods;
};

sqlite3_file* RealFile(PermissionFile* file) {
  return reinterpret_cast<sqlite3_file*>(reinterpret_cast<char*>(file) +
                                         sizeof(PermissionFile));
}

PermissionFile* PermissionFileFromBase(sqlite3_file* file) {
  return reinterpret_cast<PermissionFile*>(file);
}

void ReleaseFile(PermissionFile* file) {
  sqlite3_free_filename(file->parent_name);
  file->parent_name = nullptr;
  file->base.pMethods = nullptr;
}

int PermissionClose(sqlite3_file* file);
int PermissionRead(sqlite3_file*, void*, int, sqlite3_int64);
int PermissionWrite(sqlite3_file*, const void*, int, sqlite3_int64);
int PermissionTruncate(sqlite3_file*, sqlite3_int64);
int PermissionSync(sqlite3_file*, int);
int PermissionFileSize(sqlite3_file*, sqlite3_int64*);
int PermissionLock(sqlite3_file*, int);
int PermissionUnlock(sqlite3_file*, int);
int PermissionCheckReservedLock(sqlite3_file*, int*);
int PermissionFileControl(sqlite3_file*, int, void*);
int PermissionSectorSize(sqlite3_file*);
int PermissionDeviceCharacteristics(sqlite3_file*);
int PermissionShmMap(sqlite3_file*, int, int, int, void volatile**);
int PermissionShmLock(sqlite3_file*, int, int, int);
void PermissionShmBarrier(sqlite3_file*);
int PermissionShmUnmap(sqlite3_file*, int);
int PermissionFetch(sqlite3_file*, sqlite3_int64, int, void**);
int PermissionUnfetch(sqlite3_file*, sqlite3_int64, void*);

const sqlite3_io_methods kPermissionIoMethods = {
    3,
    PermissionClose,
    PermissionRead,
    PermissionWrite,
    PermissionTruncate,
    PermissionSync,
    PermissionFileSize,
    PermissionLock,
    PermissionUnlock,
    PermissionCheckReservedLock,
    PermissionFileControl,
    PermissionSectorSize,
    PermissionDeviceCharacteristics,
    PermissionShmMap,
    PermissionShmLock,
    PermissionShmBarrier,
    PermissionShmUnmap,
    PermissionFetch,
    PermissionUnfetch,
};

int PermissionClose(sqlite3_file* file) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  const int result =
      real->pMethods == nullptr ? SQLITE_OK : real->pMethods->xClose(real);
  ReleaseFile(permission_file);
  return result;
}

int PermissionRead(sqlite3_file* file,
                   void* buffer,
                   int amount,
                   sqlite3_int64 offset) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xRead(real, buffer, amount, offset);
}

int PermissionWrite(sqlite3_file* file,
                    const void* buffer,
                    int amount,
                    sqlite3_int64 offset) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (!permission_file->can_write) return SQLITE_PERM;
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xWrite(real, buffer, amount, offset);
}

int PermissionTruncate(sqlite3_file* file, sqlite3_int64 size) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (!permission_file->can_write) return SQLITE_PERM;
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xTruncate(real, size);
}

int PermissionSync(sqlite3_file* file, int flags) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (!permission_file->can_write) return SQLITE_PERM;
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xSync(real, flags);
}

int PermissionFileSize(sqlite3_file* file, sqlite3_int64* size) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xFileSize(real, size);
}

int PermissionLock(sqlite3_file* file, int lock) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xLock(real, lock);
}

int PermissionUnlock(sqlite3_file* file, int lock) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xUnlock(real, lock);
}

int PermissionCheckReservedLock(sqlite3_file* file, int* result) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xCheckReservedLock(real, result);
}

bool FileControlRequiresWrite(int operation) {
  switch (operation) {
    case SQLITE_FCNTL_SIZE_HINT:
    case SQLITE_FCNTL_OVERWRITE:
    case SQLITE_FCNTL_SYNC:
    case SQLITE_FCNTL_COMMIT_PHASETWO:
    case SQLITE_FCNTL_BEGIN_ATOMIC_WRITE:
    case SQLITE_FCNTL_COMMIT_ATOMIC_WRITE:
    case SQLITE_FCNTL_ROLLBACK_ATOMIC_WRITE:
      return true;
    default:
      return false;
  }
}

int PermissionFileControl(sqlite3_file* file, int operation, void* argument) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (FileControlRequiresWrite(operation) && !permission_file->can_write) {
    return SQLITE_PERM;
  }

  if (operation == SQLITE_FCNTL_SET_LOCKPROXYFILE && argument != nullptr) {
    const char* proxy_path = static_cast<const char*>(argument);
    const bool allowed =
        strcmp(proxy_path, ":auto:") == 0
            ? permission_file->vfs->AllowsTemporaryFile(true, true)
            : permission_file->vfs->AllowsPath(proxy_path, true, true);
    if (!allowed) return SQLITE_PERM;
  }

  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xFileControl(real, operation, argument);
}

int PermissionSectorSize(sqlite3_file* file) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xSectorSize(real);
}

int PermissionDeviceCharacteristics(sqlite3_file* file) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xDeviceCharacteristics(real);
}

int PermissionShmMap(sqlite3_file* file,
                     int page,
                     int page_size,
                     int extend,
                     void volatile** result) {
  PermissionFile* permission_file = PermissionFileFromBase(file);

  // OS VFS implementations can open or create the SHM file read-write even
  // when extend is false. A read-only clone of the SQLite filename is used
  // when the caller has no permission to write sidecars.
  if (!permission_file->can_write_sidecar &&
      permission_file->parent_name == nullptr) {
    return SQLITE_PERM;
  }

  sqlite3_file* real = RealFile(permission_file);
  if (real->pMethods->xShmMap == nullptr) return SQLITE_NOTFOUND;
  return real->pMethods->xShmMap(real, page, page_size, extend, result);
}

int PermissionShmLock(sqlite3_file* file, int offset, int amount, int flags) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  if (real->pMethods->xShmLock == nullptr) return SQLITE_NOTFOUND;
  return real->pMethods->xShmLock(real, offset, amount, flags);
}

void PermissionShmBarrier(sqlite3_file* file) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  if (real->pMethods->xShmBarrier != nullptr) {
    real->pMethods->xShmBarrier(real);
  }
}

int PermissionShmUnmap(sqlite3_file* file, int delete_flag) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (delete_flag && !permission_file->can_write_sidecar) return SQLITE_PERM;
  sqlite3_file* real = RealFile(permission_file);
  if (real->pMethods->xShmUnmap == nullptr) return SQLITE_NOTFOUND;
  return real->pMethods->xShmUnmap(real, delete_flag);
}

int PermissionFetch(sqlite3_file* file,
                    sqlite3_int64 offset,
                    int amount,
                    void** result) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  if (real->pMethods->xFetch == nullptr) {
    *result = nullptr;
    return SQLITE_OK;
  }
  return real->pMethods->xFetch(real, offset, amount, result);
}

int PermissionUnfetch(sqlite3_file* file, sqlite3_int64 offset, void* page) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  if (real->pMethods->xUnfetch == nullptr) return SQLITE_OK;
  return real->pMethods->xUnfetch(real, offset, page);
}

SQLitePermissionVFS* Self(sqlite3_vfs* vfs) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData);
}

int PermissionVfsOpen(sqlite3_vfs* vfs,
                      sqlite3_filename name,
                      sqlite3_file* file,
                      int flags,
                      int* out_flags) {
  return Self(vfs)->Open(name, file, flags, out_flags);
}

int PermissionVfsDelete(sqlite3_vfs* vfs, const char* name, int sync_dir) {
  return Self(vfs)->Delete(name, sync_dir);
}

int PermissionVfsAccess(sqlite3_vfs* vfs,
                        const char* name,
                        int flags,
                        int* result) {
  return Self(vfs)->Access(name, flags, result);
}

int PermissionVfsFullPathname(sqlite3_vfs* vfs,
                              const char* name,
                              int length,
                              char* output) {
  return Self(vfs)->FullPathname(name, length, output);
}

void* PermissionVfsDlOpen(sqlite3_vfs* vfs, const char* name) {
  return Self(vfs)->DlOpen(name);
}

void PermissionVfsDlError(sqlite3_vfs* vfs, int length, char* message) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  if (parent->xDlError != nullptr) parent->xDlError(parent, length, message);
}

using DlSymResult = void (*)(void);
DlSymResult PermissionVfsDlSym(sqlite3_vfs* vfs,
                               void* handle,
                               const char* symbol) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xDlSym == nullptr ? nullptr
                                   : parent->xDlSym(parent, handle, symbol);
}

void PermissionVfsDlClose(sqlite3_vfs* vfs, void* handle) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  if (parent->xDlClose != nullptr) parent->xDlClose(parent, handle);
}

int PermissionVfsRandomness(sqlite3_vfs* vfs, int length, char* output) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xRandomness(parent, length, output);
}

int PermissionVfsSleep(sqlite3_vfs* vfs, int microseconds) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xSleep(parent, microseconds);
}

int PermissionVfsCurrentTime(sqlite3_vfs* vfs, double* time) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xCurrentTime(parent, time);
}

int PermissionVfsGetLastError(sqlite3_vfs* vfs, int length, char* message) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xGetLastError == nullptr
             ? SQLITE_OK
             : parent->xGetLastError(parent, length, message);
}

int PermissionVfsCurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* time) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xCurrentTimeInt64(parent, time);
}

int PermissionVfsSetSystemCall(sqlite3_vfs* vfs,
                               const char* name,
                               sqlite3_syscall_ptr callback) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xSetSystemCall(parent, name, callback);
}

sqlite3_syscall_ptr PermissionVfsGetSystemCall(sqlite3_vfs* vfs,
                                               const char* name) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xGetSystemCall(parent, name);
}

const char* PermissionVfsNextSystemCall(sqlite3_vfs* vfs, const char* name) {
  sqlite3_vfs* parent = Self(vfs)->parent();
  return parent->xNextSystemCall(parent, name);
}

}  // namespace

SQLitePermissionVFS::SQLitePermissionVFS(Environment* env)
    : parent_(sqlite3_vfs_find(nullptr)),
      memory_vfs_(sqlite3_vfs_find("memdb")),
      env_(env),
      permission_(env->permission()),
      name_("node-permission-" + std::to_string(++next_vfs_id)) {
  CHECK_NOT_NULL(parent_);
  CHECK_NOT_NULL(memory_vfs_);

  vfs_.iVersion = parent_->iVersion;
  vfs_.szOsFile = sizeof(PermissionFile) +
                  std::max(parent_->szOsFile, memory_vfs_->szOsFile);
  vfs_.mxPathname = parent_->mxPathname;
  vfs_.zName = name_.c_str();
  vfs_.pAppData = this;
  vfs_.xOpen = PermissionVfsOpen;
  vfs_.xDelete = PermissionVfsDelete;
  vfs_.xAccess = PermissionVfsAccess;
  vfs_.xFullPathname = PermissionVfsFullPathname;
  vfs_.xDlOpen = PermissionVfsDlOpen;
  vfs_.xDlError = PermissionVfsDlError;
  vfs_.xDlSym = PermissionVfsDlSym;
  vfs_.xDlClose = PermissionVfsDlClose;
  vfs_.xRandomness = PermissionVfsRandomness;
  vfs_.xSleep = PermissionVfsSleep;
  vfs_.xCurrentTime = PermissionVfsCurrentTime;
  vfs_.xGetLastError = PermissionVfsGetLastError;
  vfs_.xCurrentTimeInt64 = parent_->xCurrentTimeInt64 == nullptr
                               ? nullptr
                               : PermissionVfsCurrentTimeInt64;
  vfs_.xSetSystemCall =
      parent_->xSetSystemCall == nullptr ? nullptr : PermissionVfsSetSystemCall;
  vfs_.xGetSystemCall =
      parent_->xGetSystemCall == nullptr ? nullptr : PermissionVfsGetSystemCall;
  vfs_.xNextSystemCall = parent_->xNextSystemCall == nullptr
                             ? nullptr
                             : PermissionVfsNextSystemCall;
}

SQLitePermissionVFS::~SQLitePermissionVFS() {
  if (registered_) sqlite3_vfs_unregister(&vfs_);
}

bool SQLitePermissionVFS::Register() {
  if (registered_) return true;
  registered_ = sqlite3_vfs_register(&vfs_, 0) == SQLITE_OK;
  return registered_;
}

bool SQLitePermissionVFS::AllowsCanonicalPath(std::string_view path,
                                              bool read,
                                              bool write) const {
  // Called on SQLite worker threads too, so denials are not published.
  return (!read ||
          permission_->is_granted_quiet(
              env_, permission::PermissionScope::kFileSystemRead, path)) &&
         (!write ||
          permission_->is_granted_quiet(
              env_, permission::PermissionScope::kFileSystemWrite, path));
}

bool SQLitePermissionVFS::AllowsPath(const char* path,
                                     bool read,
                                     bool write) const {
  if (permission_->warning_only()) return true;

  const std::string canonical = CanonicalPath(this, path);
  return !canonical.empty() && AllowsCanonicalPath(canonical, read, write);
}

bool SQLitePermissionVFS::AllowsSidecarPath(const char* path,
                                            bool read,
                                            bool write) const {
  if (permission_->warning_only()) return true;

  const std::string canonical = CanonicalPath(this, path);
  if (canonical.empty()) return false;
  if (AllowsCanonicalPath(canonical, read, write)) return true;

  std::string database_path;
  return IsSidecarPath(canonical, &database_path) &&
         AllowsCanonicalPath(database_path, read, write);
}

bool SQLitePermissionVFS::AllowsTemporaryFile(bool read, bool write) const {
  // SQLite does not provide a pathname for anonymous temporary files. Disk
  // storage is allowed only with global permissions. Otherwise xOpen uses
  // SQLite's memory VFS.
  return permission_->warning_only() || AllowsCanonicalPath("", read, write);
}

int SQLitePermissionVFS::Open(sqlite3_filename name,
                              sqlite3_file* file,
                              int flags,
                              int* out_flags) {
  auto* permission_file = reinterpret_cast<PermissionFile*>(file);
  memset(permission_file, 0, vfs_.szOsFile);
  permission_file->vfs = this;

  const bool read =
      (flags & (SQLITE_OPEN_READONLY | SQLITE_OPEN_READWRITE)) != 0;
  bool write = (flags & SQLITE_OPEN_READWRITE) != 0;
  int parent_flags = flags;
  sqlite3_vfs* selected_vfs = parent_;
  bool can_write_sidecar = write;

  if (flags & SQLITE_OPEN_MEMORY) {
    selected_vfs = memory_vfs_;
  } else if (name == nullptr) {
    if (!AllowsTemporaryFile(read, write)) selected_vfs = memory_vfs_;
  } else {
    const char* permission_name = name;
    const bool is_journal =
        flags & (SQLITE_OPEN_MAIN_JOURNAL | SQLITE_OPEN_WAL);
    if (is_journal) {
      permission_name = sqlite3_filename_database(name);
      sqlite3_file* database_file = sqlite3_database_file_object(name);
      if (database_file != nullptr &&
          !PermissionFileFromBase(database_file)->can_write_sidecar) {
        write = false;
        parent_flags &= ~(SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
        parent_flags |= SQLITE_OPEN_READONLY;
      }
    }

    const bool allowed =
        permission_name != nullptr &&
        ((flags & SQLITE_OPEN_SUPER_JOURNAL)
             ? AllowsSidecarPath(permission_name, read, write)
             : AllowsPath(permission_name, read, write));
    if (!allowed) return SQLITE_PERM;

    if (flags & SQLITE_OPEN_MAIN_DB) {
      can_write_sidecar = write || AllowsPath(permission_name, false, true);
      if (!can_write_sidecar) {
        permission_file->parent_name = CreateReadOnlyShmFilename(name);
        if (permission_file->parent_name == nullptr) return SQLITE_NOMEM;
      }
    }
  }

  sqlite3_filename parent_name = permission_file->parent_name == nullptr
                                     ? name
                                     : permission_file->parent_name;
  sqlite3_file* real = RealFile(permission_file);
  const int result = selected_vfs->xOpen(
      selected_vfs, parent_name, real, parent_flags, out_flags);
  if (real->pMethods == nullptr) {
    ReleaseFile(permission_file);
    return result;
  }

  permission_file->can_write =
      write &&
      (out_flags == nullptr || (*out_flags & SQLITE_OPEN_READONLY) == 0);
  permission_file->can_write_sidecar = can_write_sidecar;
  permission_file->methods = kPermissionIoMethods;
  permission_file->methods.iVersion =
      std::min(permission_file->methods.iVersion, real->pMethods->iVersion);
  if (permission_file->methods.iVersion < 2) {
    permission_file->methods.xShmMap = nullptr;
    permission_file->methods.xShmLock = nullptr;
    permission_file->methods.xShmBarrier = nullptr;
    permission_file->methods.xShmUnmap = nullptr;
  }
  if (permission_file->methods.iVersion < 3) {
    permission_file->methods.xFetch = nullptr;
    permission_file->methods.xUnfetch = nullptr;
  }
  file->pMethods = &permission_file->methods;
  return result;
}

int SQLitePermissionVFS::Delete(const char* name, int sync_dir) {
  if (name == nullptr || !AllowsSidecarPath(name, false, true)) {
    return SQLITE_PERM;
  }
  return parent_->xDelete(parent_, name, sync_dir);
}

int SQLitePermissionVFS::Access(const char* name, int flags, int* result) {
  if (name == nullptr) {
    *result = 0;
    return SQLITE_OK;
  }

  if (!AllowsSidecarPath(name, true, flags == SQLITE_ACCESS_READWRITE)) {
    *result = 0;
    return SQLITE_OK;
  }
  return parent_->xAccess(parent_, name, flags, result);
}

int SQLitePermissionVFS::FullPathname(const char* name,
                                      int length,
                                      char* output) const {
#ifdef _WIN32
  return parent_->xFullPathname(parent_, name, length, output);
#else
  // The parent VFS also resolves symbolic links, but the permission model
  // checks lexically resolved paths. Resolve the same way so that the path
  // SQLite opens is the path that was checked.
  const std::string resolved = PathResolve(env_, {name});
  if (resolved.size() >= static_cast<size_t>(length)) return SQLITE_CANTOPEN;
  memcpy(output, resolved.c_str(), resolved.size() + 1);
  return SQLITE_OK;
#endif
}

void* SQLitePermissionVFS::DlOpen(const char* name) {
  if (!permission_->warning_only()) return nullptr;
  return parent_->xDlOpen == nullptr ? nullptr
                                     : parent_->xDlOpen(parent_, name);
}

}  // namespace sqlite
}  // namespace node
