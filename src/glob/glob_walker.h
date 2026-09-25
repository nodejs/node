#ifndef SRC_GLOB_GLOB_WALKER_H_
#define SRC_GLOB_GLOB_WALKER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdint>
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

// One call a walk off the main thread needs made to the exclude callback,
// which only that thread can do. A directory's questions are recorded in
// the order the callback would see them, together with what decides
// whether each is asked at all, so the main thread can put exactly the
// calls the walk on that thread would have made.
struct Question {
  static constexpr size_t kNoEntry = SIZE_MAX;
  // ExcludesEntry(first, parent, type) when set, else ExcludesPath(first)
  bool entry;
  std::string first;
  std::string parent;
  int type;
  // The listing entry the question belongs to; kNoEntry for none
  size_t entry_id;
  // For an entry question: whether "not excluded" makes the walk descend
  // into the entry, which raises the entry's path question. For a path
  // question: whether the walk descends regardless of the entry's answers.
  bool descends;
};

struct Answer {
  bool entry;
  bool excluded;
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

  // A slice that stopped because a directory needs the exclude callback:
  // the questions for the main thread, then the answers to resume with.
  bool HasQuestions() const;
  const std::vector<Question>& questions() const;
  void SetAnswers(std::vector<Answer> answers);

  // Paths the permission model denied off the main thread since the last
  // call, for publishing from that thread
  void TakeDenied(std::vector<std::string>* out);

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
