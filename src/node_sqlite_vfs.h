#ifndef SRC_NODE_SQLITE_VFS_H_
#define SRC_NODE_SQLITE_VFS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "sqlite3.h"

#include <string>
#include <string_view>

namespace node {

class Environment;

namespace permission {
class Permission;
}

namespace sqlite {

using SQLiteDlSym = void (*)(void);

// Returns true if path contains a SQLite URI parameter named "vfs".
// URI-selected VFSes must not be allowed to bypass the permission VFS.
bool HasSQLiteVfsUriParameter(std::string_view path);
// Returns true if a SQLite URI parameter has the specified decoded value.
bool SQLiteUriParameterEquals(std::string_view path,
                              std::string_view parameter,
                              std::string_view value);
// Returns true if the first matching SQLite URI parameter is a true boolean.
bool SQLiteUriBooleanParameter(std::string_view path,
                               std::string_view parameter);
// Returns the decoded filesystem path portion of a SQLite URI filename. Plain
// filenames are returned unchanged.
std::string SQLitePathForPermission(std::string_view path);

class SQLitePermissionVFS {
 public:
  explicit SQLitePermissionVFS(Environment* env);
  ~SQLitePermissionVFS();

  SQLitePermissionVFS(const SQLitePermissionVFS&) = delete;
  SQLitePermissionVFS& operator=(const SQLitePermissionVFS&) = delete;

  bool Register();
  void Unregister();

  const std::string& name() const { return name_; }
  bool registered() const { return registered_; }
  sqlite3_vfs* parent() const { return parent_; }

  int Open(sqlite3_filename name,
           sqlite3_file* file,
           int flags,
           int* out_flags);
  int Delete(const char* name, int sync_dir);
  int Access(const char* name, int flags, int* result);
  int FullPathname(const char* name, int length, char* output);
  void* DlOpen(const char* name);
  void DlError(int length, char* message);
  SQLiteDlSym DlSym(void* handle, const char* symbol);
  void DlClose(void* handle);
  int Randomness(int length, char* output);
  int Sleep(int microseconds);
  int CurrentTime(double* time);
  int GetLastError(int length, char* message);
  int CurrentTimeInt64(sqlite3_int64* time);
  int SetSystemCall(const char* name, sqlite3_syscall_ptr callback);
  sqlite3_syscall_ptr GetSystemCall(const char* name);
  const char* NextSystemCall(const char* name);

  bool AllowsPath(std::string_view path, bool read, bool write) const;
  bool AllowsSidecarPath(std::string_view path, bool read, bool write) const;
  bool AllowsTemporaryFile(bool read, bool write) const;

 private:
  sqlite3_vfs vfs_{};
  sqlite3_vfs* parent_ = nullptr;
  sqlite3_vfs* memory_vfs_ = nullptr;
  permission::Permission* permission_ = nullptr;
  std::string name_;
  bool registered_ = false;
};

}  // namespace sqlite
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_SQLITE_VFS_H_
