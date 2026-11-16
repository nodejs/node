#ifndef SRC_NODE_SQLITE_VFS_H_
#define SRC_NODE_SQLITE_VFS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_mutex.h"
#include "permission/permission_base.h"
#include "sqlite3.h"

#include <optional>
#include <string>
#include <string_view>

namespace node {

class Environment;

namespace permission {
class Permission;
}

namespace sqlite {

class SQLitePermissionVFS {
 public:
  struct Denial {
    permission::PermissionScope scope;
    std::string path;
  };

  explicit SQLitePermissionVFS(Environment* env);
  ~SQLitePermissionVFS();

  SQLitePermissionVFS(const SQLitePermissionVFS&) = delete;
  SQLitePermissionVFS& operator=(const SQLitePermissionVFS&) = delete;

  bool Register();

  const std::string& name() const { return name_; }
  sqlite3_vfs* parent() const { return parent_; }

  int Open(sqlite3_filename name,
           sqlite3_file* file,
           int flags,
           int* out_flags);
  int Delete(const char* name, int sync_dir);
  int Access(const char* name, int flags, int* result);
  int FullPathname(const char* name, int length, char* output) const;
  void* DlOpen(const char* name);

  // The denial behind a failed SQLite call, if errcode is SQLITE_PERM.
  std::optional<Denial> TakeDenied(int errcode);

  bool AllowsPath(const char* path, bool read, bool write) const;
  bool AllowsTemporaryFile(bool read, bool write) const;

 private:
  bool AllowsSidecarPath(const char* path, bool read, bool write) const;
  bool AllowsDatabase(const char* path, bool read, bool write);
  bool AllowsCanonicalPath(std::string_view path, bool read, bool write) const;

  sqlite3_vfs vfs_{};
  sqlite3_vfs* parent_ = nullptr;
  sqlite3_vfs* memory_vfs_ = nullptr;
  Environment* env_ = nullptr;
  permission::Permission* permission_ = nullptr;
  std::string name_;
  bool registered_ = false;
  Mutex denied_mutex_;
  std::optional<Denial> denied_;
};

}  // namespace sqlite
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_NODE_SQLITE_VFS_H_
