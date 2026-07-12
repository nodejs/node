#include "node_sqlite_vfs.h"

#include "env-inl.h"
#include "permission/permission.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace node {
namespace sqlite {

namespace {

std::atomic<uint64_t> next_vfs_id{0};

int HexValue(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

void DecodeUriComponent(std::string_view component, std::string* decoded) {
  decoded->clear();
  decoded->reserve(component.size());
  for (size_t i = 0; i < component.size(); ++i) {
    if (component[i] == '%' && i + 2 < component.size()) {
      const int high = HexValue(component[i + 1]);
      const int low = HexValue(component[i + 2]);
      if (high >= 0 && low >= 0) {
        const char value = static_cast<char>((high << 4) | low);
        if (value == '\0') break;
        decoded->push_back(value);
        i += 2;
        continue;
      }
    }
    decoded->push_back(component[i]);
  }
}

bool GetSQLiteUriParameter(std::string_view path,
                           std::string_view parameter,
                           bool use_last,
                           std::string* value) {
  if (!path.starts_with("file:")) return false;

  const size_t query_start = path.find('?', 5);
  const size_t fragment_start = path.find('#', 5);
  if (query_start == std::string_view::npos ||
      (fragment_start != std::string_view::npos &&
       fragment_start < query_start)) {
    return false;
  }

  const size_t query_end = fragment_start == std::string_view::npos
                               ? path.size()
                               : fragment_start;
  size_t parameter_start = query_start + 1;
  std::string decoded_key;
  std::string decoded_value;
  bool found = false;
  while (parameter_start <= query_end) {
    size_t parameter_end = path.find('&', parameter_start);
    if (parameter_end == std::string_view::npos || parameter_end > query_end) {
      parameter_end = query_end;
    }

    const size_t equals = path.find('=', parameter_start);
    const size_t key_end = equals == std::string_view::npos ||
                                   equals > parameter_end
                               ? parameter_end
                               : equals;
    DecodeUriComponent(path.substr(parameter_start, key_end - parameter_start),
                       &decoded_key);
    if (decoded_key == parameter) {
      if (equals == std::string_view::npos || equals > parameter_end) {
        value->clear();
      } else {
        DecodeUriComponent(path.substr(equals + 1, parameter_end - equals - 1),
                           &decoded_value);
        *value = decoded_value;
      }
      found = true;
      if (!use_last) return true;
    }

    if (parameter_end == query_end) break;
    parameter_start = parameter_end + 1;
  }
  return found;
}

bool EqualsIgnoreCase(std::string_view left, std::string_view right) {
  if (left.size() != right.size()) return false;
  for (size_t i = 0; i < left.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(left[i])) !=
        std::tolower(static_cast<unsigned char>(right[i]))) {
      return false;
    }
  }
  return true;
}

bool IsSidecarPath(std::string_view path, std::string* database_path) {
  constexpr std::string_view suffixes[] = {"-journal", "-wal", "-shm"};
  for (const std::string_view suffix : suffixes) {
    if (path.size() > suffix.size() && path.ends_with(suffix)) {
      *database_path = path.substr(0, path.size() - suffix.size());
      return true;
    }
  }

  // SQLite names super-journals as <database>-mj<random-suffix>. A
  // permission granted for the database also covers its super-journal.
  const size_t marker = path.rfind("-mj");
  if (marker != std::string_view::npos && marker > 0 &&
      path.size() > marker + 3) {
    *database_path = path.substr(0, marker);
    return true;
  }

  return false;
}

sqlite3_filename CreateReadOnlyShmFilename(sqlite3_filename name) {
  std::vector<const char*> parameters = {"readonly_shm", "1"};
  for (int index = 0;; ++index) {
    const char* key = sqlite3_uri_key(name, index);
    if (key == nullptr) break;
    if (strcmp(key, "readonly_shm") == 0) continue;

    const char* value = sqlite3_uri_parameter(name, key);
    parameters.push_back(key);
    parameters.push_back(value == nullptr ? "" : value);
  }

  return sqlite3_create_filename(sqlite3_filename_database(name),
                                 sqlite3_filename_journal(name),
                                 sqlite3_filename_wal(name),
                                 static_cast<int>(parameters.size() / 2),
                                 parameters.data());
}

std::string CanonicalPath(SQLitePermissionVFS* vfs, const char* path) {
  if (path == nullptr) return {};

  // SQLite normally passes a full pathname to xOpen, but xAccess and xDelete
  // may receive names produced by SQLite itself. Canonicalize those names
  // before consulting Node's path permission tree.
  std::string result(static_cast<size_t>(vfs->parent()->mxPathname) + 1, '\0');
  if (vfs->parent()->xFullPathname(vfs->parent(),
                                   path,
                                   static_cast<int>(result.size()),
                                   result.data()) != SQLITE_OK) {
    return {};
  }
  result.resize(strlen(result.c_str()));
  return result;
}

struct PermissionFile {
  sqlite3_file base;
  SQLitePermissionVFS* vfs;
  sqlite3_filename parent_name;
  bool can_read;
  bool can_write;
  bool can_write_sidecar;
  bool read_only_shm;
  sqlite3_io_methods methods;
};

sqlite3_file* RealFile(PermissionFile* file) {
  return reinterpret_cast<sqlite3_file*>(
      reinterpret_cast<char*>(file) + sizeof(PermissionFile));
}

PermissionFile* PermissionFileFromBase(sqlite3_file* file) {
  return reinterpret_cast<PermissionFile*>(file);
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

bool HasReadPermission(const PermissionFile* file) {
  return file->can_read;
}

bool HasWritePermission(const PermissionFile* file) {
  return file->can_write;
}

int PermissionClose(sqlite3_file* file) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  sqlite3_file* real = RealFile(permission_file);
  const int result = real->pMethods == nullptr
                         ? SQLITE_OK
                         : real->pMethods->xClose(real);
  sqlite3_free_filename(permission_file->parent_name);
  permission_file->parent_name = nullptr;
  file->pMethods = nullptr;
  return result;
}

int PermissionRead(sqlite3_file* file,
                   void* buffer,
                   int amount,
                   sqlite3_int64 offset) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (!HasReadPermission(permission_file)) return SQLITE_PERM;
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xRead(real, buffer, amount, offset);
}

int PermissionWrite(sqlite3_file* file,
                    const void* buffer,
                    int amount,
                    sqlite3_int64 offset) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (!HasWritePermission(permission_file)) return SQLITE_PERM;
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xWrite(real, buffer, amount, offset);
}

int PermissionTruncate(sqlite3_file* file, sqlite3_int64 size) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (!HasWritePermission(permission_file)) return SQLITE_PERM;
  sqlite3_file* real = RealFile(permission_file);
  return real->pMethods->xTruncate(real, size);
}

int PermissionSync(sqlite3_file* file, int flags) {
  PermissionFile* permission_file = PermissionFileFromBase(file);
  if (!HasWritePermission(permission_file)) return SQLITE_PERM;
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
  if (FileControlRequiresWrite(operation) &&
      !HasWritePermission(permission_file)) {
    return SQLITE_PERM;
  }

  if (operation == SQLITE_FCNTL_SET_LOCKPROXYFILE && argument != nullptr) {
    const std::string_view proxy_path = static_cast<const char*>(argument);
    const bool allowed =
        proxy_path == ":auto:"
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
  if (!HasReadPermission(permission_file)) return SQLITE_PERM;

  // OS VFS implementations can open or create the SHM file read-write even
  // when extend is false. A read-only clone of the SQLite filename is used
  // when the caller has no permission to write sidecars.
  if (!permission_file->can_write_sidecar &&
      !permission_file->read_only_shm) {
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
  if (!HasReadPermission(permission_file)) return SQLITE_PERM;
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

int PermissionVfsOpen(sqlite3_vfs* vfs,
                      sqlite3_filename name,
                      sqlite3_file* file,
                      int flags,
                      int* out_flags) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->Open(
      name, file, flags, out_flags);
}

int PermissionVfsDelete(sqlite3_vfs* vfs, const char* name, int sync_dir) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->Delete(name,
                                                                   sync_dir);
}

int PermissionVfsAccess(sqlite3_vfs* vfs,
                        const char* name,
                        int flags,
                        int* result) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->Access(name,
                                                                   flags,
                                                                   result);
}

int PermissionVfsFullPathname(sqlite3_vfs* vfs,
                              const char* name,
                              int length,
                              char* output) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->FullPathname(
      name, length, output);
}

void* PermissionVfsDlOpen(sqlite3_vfs* vfs, const char* name) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->DlOpen(name);
}

void PermissionVfsDlError(sqlite3_vfs* vfs, int length, char* message) {
  static_cast<SQLitePermissionVFS*>(vfs->pAppData)->DlError(length, message);
}

SQLiteDlSym PermissionVfsDlSym(sqlite3_vfs* vfs,
                               void* handle,
                               const char* symbol) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->DlSym(handle,
                                                                  symbol);
}

void PermissionVfsDlClose(sqlite3_vfs* vfs, void* handle) {
  static_cast<SQLitePermissionVFS*>(vfs->pAppData)->DlClose(handle);
}

int PermissionVfsRandomness(sqlite3_vfs* vfs, int length, char* output) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->Randomness(length,
                                                                       output);
}

int PermissionVfsSleep(sqlite3_vfs* vfs, int microseconds) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->Sleep(microseconds);
}

int PermissionVfsCurrentTime(sqlite3_vfs* vfs, double* time) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->CurrentTime(time);
}

int PermissionVfsGetLastError(sqlite3_vfs* vfs, int length, char* message) {
  auto* permission_vfs =
      static_cast<SQLitePermissionVFS*>(vfs->pAppData);
  return permission_vfs->GetLastError(length, message);
}

int PermissionVfsCurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* time) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->CurrentTimeInt64(
      time);
}

int PermissionVfsSetSystemCall(sqlite3_vfs* vfs,
                               const char* name,
                               sqlite3_syscall_ptr callback) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->SetSystemCall(
      name, callback);
}

sqlite3_syscall_ptr PermissionVfsGetSystemCall(sqlite3_vfs* vfs,
                                               const char* name) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->GetSystemCall(name);
}

const char* PermissionVfsNextSystemCall(sqlite3_vfs* vfs, const char* name) {
  return static_cast<SQLitePermissionVFS*>(vfs->pAppData)->NextSystemCall(
      name);
}

}  // namespace

bool HasSQLiteVfsUriParameter(std::string_view path) {
  std::string value;
  return GetSQLiteUriParameter(path, "vfs", false, &value);
}

bool SQLiteUriParameterEquals(std::string_view path,
                              std::string_view parameter,
                              std::string_view value) {
  std::string actual;
  return GetSQLiteUriParameter(path, parameter, true, &actual) &&
         actual == value;
}

bool SQLiteUriBooleanParameter(std::string_view path,
                               std::string_view parameter) {
  std::string value;
  if (!GetSQLiteUriParameter(path, parameter, false, &value)) return false;

  if (!value.empty() && std::isdigit(static_cast<unsigned char>(value[0]))) {
    return std::strtoll(value.c_str(), nullptr, 10) != 0;
  }
  return EqualsIgnoreCase(value, "on") || EqualsIgnoreCase(value, "yes") ||
         EqualsIgnoreCase(value, "true");
}

std::string SQLitePathForPermission(std::string_view path) {
  if (!path.starts_with("file:")) return std::string(path);

  const size_t path_end = path.find_first_of("?#", 5);
  std::string decoded;
  DecodeUriComponent(
      path.substr(5,
                  path_end == std::string_view::npos
                      ? std::string_view::npos
                      : path_end - 5),
      &decoded);

  if (decoded.starts_with("//")) {
    const size_t pathname_start = decoded.find('/', 2);
    const std::string_view authority =
        pathname_start == std::string::npos
            ? std::string_view(decoded).substr(2)
            : std::string_view(decoded).substr(2, pathname_start - 2);
    if (authority.empty() || authority == "localhost") {
      decoded = pathname_start == std::string::npos
                    ? std::string()
                    : decoded.substr(pathname_start);
    }
  }

#ifdef _WIN32
  if (decoded.size() >= 3 && decoded[0] == '/' &&
      std::isalpha(static_cast<unsigned char>(decoded[1])) &&
      decoded[2] == ':') {
    decoded.erase(0, 1);
  }
#endif

  return decoded;
}

SQLitePermissionVFS::SQLitePermissionVFS(Environment* env)
    : parent_(sqlite3_vfs_find(nullptr)),
      memory_vfs_(sqlite3_vfs_find("memdb")),
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
  vfs_.xSetSystemCall = parent_->xSetSystemCall == nullptr
                            ? nullptr
                            : PermissionVfsSetSystemCall;
  vfs_.xGetSystemCall = parent_->xGetSystemCall == nullptr
                            ? nullptr
                            : PermissionVfsGetSystemCall;
  vfs_.xNextSystemCall = parent_->xNextSystemCall == nullptr
                             ? nullptr
                             : PermissionVfsNextSystemCall;
}

SQLitePermissionVFS::~SQLitePermissionVFS() {
  Unregister();
}

bool SQLitePermissionVFS::Register() {
  if (registered_) return true;
  registered_ = sqlite3_vfs_register(&vfs_, 0) == SQLITE_OK;
  return registered_;
}

void SQLitePermissionVFS::Unregister() {
  if (!registered_) return;
  sqlite3_vfs_unregister(&vfs_);
  registered_ = false;
}

bool SQLitePermissionVFS::AllowsPath(std::string_view path,
                                     bool read,
                                     bool write) const {
  if (permission_->warning_only()) return true;

  const std::string canonical = CanonicalPath(
      const_cast<SQLitePermissionVFS*>(this), std::string(path).c_str());
  if (canonical.empty()) return false;

  const auto allowed = [&](std::string_view candidate) {
    return (!read || permission_->is_granted_no_side_effects(
                          permission::PermissionScope::kFileSystemRead,
                          candidate)) &&
           (!write || permission_->is_granted_no_side_effects(
                           permission::PermissionScope::kFileSystemWrite,
                           candidate));
  };

  return allowed(canonical);
}

bool SQLitePermissionVFS::AllowsSidecarPath(std::string_view path,
                                            bool read,
                                            bool write) const {
  if (AllowsPath(path, read, write)) return true;

  const std::string canonical = CanonicalPath(
      const_cast<SQLitePermissionVFS*>(this), std::string(path).c_str());
  if (canonical.empty()) return false;

  std::string database_path;
  return IsSidecarPath(canonical, &database_path) &&
         AllowsPath(database_path, read, write);
}

bool SQLitePermissionVFS::AllowsTemporaryFile(bool read, bool write) const {
  if (permission_->warning_only()) return true;

  // SQLite does not provide a pathname for anonymous temporary files. Disk
  // storage is allowed only with global permissions. Otherwise xOpen uses
  // SQLite's memory VFS.
  return (!read || permission_->is_granted_no_side_effects(
                         permission::PermissionScope::kFileSystemRead, "")) &&
         (!write || permission_->is_granted_no_side_effects(
                          permission::PermissionScope::kFileSystemWrite, ""));
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
  const bool immutable = name != nullptr && (flags & SQLITE_OPEN_MAIN_DB) &&
                         sqlite3_uri_boolean(name, "immutable", 0) != 0;
  bool write = (flags & SQLITE_OPEN_READWRITE) != 0 && !immutable;
  int parent_flags = flags;
  sqlite3_vfs* selected_vfs = parent_;
  bool can_write_sidecar = write;
  bool read_only_shm = false;
  bool allowed;

  if (flags & SQLITE_OPEN_MEMORY) {
    selected_vfs = memory_vfs_;
    allowed = true;
  } else if (name == nullptr) {
    allowed = AllowsTemporaryFile(read, write);
    if (!allowed) {
      selected_vfs = memory_vfs_;
      allowed = true;
    }
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

    allowed = permission_name != nullptr &&
              !HasSQLiteVfsUriParameter(permission_name) &&
              ((flags & SQLITE_OPEN_SUPER_JOURNAL)
                   ? AllowsSidecarPath(permission_name, read, write)
                   : AllowsPath(permission_name, read, write));
    if (allowed && permission_name != nullptr &&
        (flags & SQLITE_OPEN_MAIN_DB)) {
      can_write_sidecar = AllowsPath(permission_name, false, true);
      if (!can_write_sidecar) {
        permission_file->parent_name = CreateReadOnlyShmFilename(name);
        if (permission_file->parent_name == nullptr) return SQLITE_NOMEM;
        read_only_shm = true;
      }
    }
  }

  if (!allowed) return SQLITE_PERM;

  sqlite3_filename parent_name = permission_file->parent_name == nullptr
                                     ? name
                                     : permission_file->parent_name;
  sqlite3_file* real = RealFile(permission_file);
  const int result = selected_vfs->xOpen(
      selected_vfs, parent_name, real, parent_flags, out_flags);
  if (real->pMethods == nullptr) {
    sqlite3_free_filename(permission_file->parent_name);
    permission_file->parent_name = nullptr;
    file->pMethods = nullptr;
    return result;
  }

  permission_file->can_read = read;
  permission_file->can_write = write &&
                               (out_flags == nullptr ||
                                (*out_flags & SQLITE_OPEN_READONLY) == 0);
  permission_file->can_write_sidecar = can_write_sidecar;
  permission_file->read_only_shm = read_only_shm;
  permission_file->methods = kPermissionIoMethods;
  permission_file->methods.iVersion = std::min(
      permission_file->methods.iVersion, real->pMethods->iVersion);
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

  const bool read = true;
  const bool write = flags == SQLITE_ACCESS_READWRITE;
  if (!AllowsSidecarPath(name, read, write)) {
    *result = 0;
    return SQLITE_OK;
  }
  return parent_->xAccess(parent_, name, flags, result);
}

int SQLitePermissionVFS::FullPathname(const char* name,
                                      int length,
                                      char* output) {
  return parent_->xFullPathname(parent_, name, length, output);
}

void* SQLitePermissionVFS::DlOpen(const char* name) {
  if (!permission_->warning_only()) return nullptr;
  return parent_->xDlOpen == nullptr
             ? nullptr
             : parent_->xDlOpen(parent_, name);
}

void SQLitePermissionVFS::DlError(int length, char* message) {
  if (parent_->xDlError != nullptr) {
    parent_->xDlError(parent_, length, message);
  }
}

SQLiteDlSym SQLitePermissionVFS::DlSym(void* handle, const char* symbol) {
  return parent_->xDlSym == nullptr
             ? nullptr
             : parent_->xDlSym(parent_, handle, symbol);
}

void SQLitePermissionVFS::DlClose(void* handle) {
  if (parent_->xDlClose != nullptr) parent_->xDlClose(parent_, handle);
}

int SQLitePermissionVFS::Randomness(int length, char* output) {
  return parent_->xRandomness(parent_, length, output);
}

int SQLitePermissionVFS::Sleep(int microseconds) {
  return parent_->xSleep(parent_, microseconds);
}

int SQLitePermissionVFS::CurrentTime(double* time) {
  return parent_->xCurrentTime(parent_, time);
}

int SQLitePermissionVFS::GetLastError(int length, char* message) {
  return parent_->xGetLastError == nullptr
             ? SQLITE_OK
             : parent_->xGetLastError(parent_, length, message);
}

int SQLitePermissionVFS::CurrentTimeInt64(sqlite3_int64* time) {
  return parent_->xCurrentTimeInt64(parent_, time);
}

int SQLitePermissionVFS::SetSystemCall(const char* name,
                                       sqlite3_syscall_ptr callback) {
  return parent_->xSetSystemCall(parent_, name, callback);
}

sqlite3_syscall_ptr SQLitePermissionVFS::GetSystemCall(const char* name) {
  return parent_->xGetSystemCall(parent_, name);
}

const char* SQLitePermissionVFS::NextSystemCall(const char* name) {
  return parent_->xNextSystemCall(parent_, name);
}

}  // namespace sqlite
}  // namespace node
