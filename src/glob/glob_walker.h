#ifndef SRC_GLOB_GLOB_WALKER_H_
#define SRC_GLOB_GLOB_WALKER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "glob/glob_program.h"
#include "uv.h"

namespace node {
class Environment;
}  // namespace node

namespace node::glob {

class ExcludeFilter {
 public:
  virtual ~ExcludeFilter() = default;
  // A path relative to the walk's cwd.
  virtual bool ExcludesPath(std::string_view path) = 0;
  // A directory entry: its name, the absolute path of the directory holding
  // it, and its uv_dirent_type_t.
  virtual bool ExcludesEntry(std::string_view name,
                             std::string_view parent_path,
                             int type) = 0;
  // Set once a call has thrown. The walk stops and the exception is left
  // for the caller to propagate.
  virtual bool failed() const = 0;
};

struct WalkOptions {
  // options.cwd, or ".": the string paths are resolved against.
  std::string cwd = ".";
  bool follow_symlinks = false;
  bool with_file_types = false;
  // options.maxDepth: how many directory levels below cwd a result may
  // sit at (cwd itself is 0), or kNoDepthLimit.
  static constexpr size_t kNoDepthLimit = std::numeric_limits<size_t>::max();
  size_t max_depth = kNoDepthLimit;

  ExcludeFilter* exclude_filter = nullptr;
};

struct WalkEntry {
  std::string path;
  int type = UV_DIRENT_UNKNOWN;
};

class WalkerImpl;

class Walk {
 public:
  Walk(Environment* env,
       const WalkOptions& options,
       const std::vector<CompiledPatternPtr>& includes,
       const std::vector<CompiledPatternPtr>& excludes);
  ~Walk();

  // Advances the traversal until at least `max_results` new results are
  // available or the walk is over
  bool RunSlice(size_t max_results,
                bool on_main_thread,
                std::vector<WalkEntry>* out);

  // Stops background scanning and joins the walk's threads. Idempotent;
  // safe while a slice is in flight (it will wind down promptly). Called
  // when the walk finishes or is cancelled so threads never wait for
  // garbage collection.
  void Stop();

 private:
  std::unique_ptr<WalkerImpl> impl_;
};

// Runs a walk to completion.
void GlobSync(Environment* env,
              const WalkOptions& options,
              const std::vector<CompiledPatternPtr>& includes,
              const std::vector<CompiledPatternPtr>& excludes,
              std::vector<WalkEntry>* out);

}  // namespace node::glob

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_GLOB_GLOB_WALKER_H_
