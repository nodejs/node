#include "glob/glob_walker.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "env-inl.h"
#include "glob/glob_matcher.h"
#include "glob/glob_parser.h"
#include "glob/glob_unicode.h"
#include "node_file-inl.h"
#include "node_mutex.h"
#include "path.h"
#include "permission/permission.h"
#include "simdutf.h"

namespace node::glob {

namespace {
// A walk always runs under the host's own path rules, unlike matching,
// where the caller picks the platform. Fixing this at compile time lets
// every separator branch below fold away.
#ifdef _WIN32
constexpr bool kWindows = true;
#else
constexpr bool kWindows = false;
#endif
#ifdef __APPLE__
constexpr bool kMacOS = true;
#else
constexpr bool kMacOS = false;
#endif
constexpr char kSep = kWindows ? '\\' : '/';

bool IsAbsolute(std::string_view p) {
  if (p.empty()) return false;
  if (IsPathSeparator(p[0])) return true;
  if (kWindows && p.size() >= 3 && IsWindowsDeviceRoot(p[0]) && p[1] == ':' &&
      IsPathSeparator(p[2])) {
    return true;
  }
  return false;
}

// path.normalize()
std::string Normalize(std::string_view p) {
  const std::string_view sep = kWindows ? "\\" : "/";
  if (p.empty()) return ".";
  const bool absolute = IsAbsolute(p);
  const bool trailing = IsPathSeparator(p.back());

  std::string_view body = p;
  std::string root;
  if (kWindows && p.size() >= 2 && IsWindowsDeviceRoot(p[0]) && p[1] == ':') {
    root = std::string(p.substr(0, 2));
    body = p.substr(2);
    if (!body.empty() && IsPathSeparator(body[0])) {
      root += sep;
      body = body.substr(1);
    }
  } else if (absolute) {
    root = std::string(sep);
    body = p.substr(1);
    // Windows UNC paths keep their leading double separator.
    if (kWindows && !body.empty() && IsPathSeparator(body[0])) {
      root += sep;
      body = body.substr(1);
    }
  }

  std::string normalized =
      NormalizeString(body, /*allowAboveRoot=*/root.empty(), sep);
  if (normalized.empty() && root.empty()) normalized = ".";
  if (!normalized.empty() && trailing) normalized += sep;
  return root + normalized;
}

// path.join()
std::string Join(std::string_view a, std::string_view b) {
  if (a.empty()) return b.empty() ? std::string(".") : Normalize(b);
  if (b.empty()) return Normalize(a);
  std::string joined(a);
  joined += kWindows ? '\\' : '/';
  joined += b;
  return Normalize(joined);
}

std::string JoinEntry(const std::string& dir, const std::string& name) {
  if (dir == ".") return name;
  std::string out;
  out.reserve(dir.size() + name.size() + 1);
  out += dir;
  if (!dir.empty() && !IsPathSeparator(dir.back())) {
    out += kWindows ? '\\' : '/';
  }
  out += name;
  return out;
}

// path.resolve(root, p)
std::string Resolve(const std::string& root_absolute, std::string_view p) {
  std::string resolved = IsAbsolute(p) ? Normalize(p) : Join(root_absolute, p);
  // resolve() drops trailing separators, except at a root.
  while (resolved.size() > 1 && IsPathSeparator(resolved.back()) &&
         !(kWindows && resolved.size() == 3 && resolved[1] == ':')) {
    resolved.pop_back();
  }
  return resolved;
}

// path.parse(p).root's length, for a resolved absolute path.
size_t RootLength(std::string_view p) {
  if constexpr (kWindows) {
    if (p.size() >= 2 && IsWindowsDeviceRoot(p[0]) && p[1] == ':') {
      return p.size() >= 3 && IsPathSeparator(p[2]) ? 3 : 2;
    }
    if (p.size() >= 2 && IsPathSeparator(p[0]) && IsPathSeparator(p[1])) {
      // UNC: \\server\share\ is the root.
      size_t i = 2;
      for (int part = 0; part < 2; part++) {
        while (i < p.size() && !IsPathSeparator(p[i])) i++;
        if (i < p.size()) i++;
      }
      return i;
    }
  }
  return !p.empty() && IsPathSeparator(p[0]) ? 1 : 0;
}

std::vector<std::string_view> SplitSegments(std::string_view body) {
  std::vector<std::string_view> segments;
  size_t start = 0;
  for (size_t i = 0; i <= body.size(); i++) {
    if (i == body.size() || IsPathSeparator(body[i])) {
      if (i > start) segments.push_back(body.substr(start, i - start));
      start = i + 1;
    }
  }
  return segments;
}

// String.prototype.toLowerCase()
std::string Lower(std::string_view utf8) {
  PatternString wide;
  Utf8ToUtf16(utf8, &wide);
  PatternString lowered;
  AppendLowered(wide, &lowered);
  return Utf16ToUtf8(lowered);
}

bool SegmentsEqual(std::string_view a,
                   std::string_view b,
                   bool case_sensitive) {
  return case_sensitive ? a == b : Lower(a) == Lower(b);
}

// path.relative(from, to) for two resolved absolute paths under the
// host's rules: Windows compares case-insensitively unless `case_sensitive`
// asks for the stricter form the depth limit uses to tell case variants
// apart.
std::string Relative(std::string_view from,
                     std::string_view to,
                     bool case_sensitive = !kWindows) {
  if (from == to) return "";
  const size_t from_root = RootLength(from);
  const size_t to_root = RootLength(to);
  if (!SegmentsEqual(
          from.substr(0, from_root), to.substr(0, to_root), case_sensitive)) {
    return std::string(to);  // another drive: nothing is relative
  }
  const std::vector<std::string_view> from_parts =
      SplitSegments(from.substr(from_root));
  const std::vector<std::string_view> to_parts =
      SplitSegments(to.substr(to_root));
  size_t common = 0;
  while (common < from_parts.size() && common < to_parts.size() &&
         SegmentsEqual(from_parts[common], to_parts[common], case_sensitive)) {
    common++;
  }
  std::string out;
  for (size_t i = common; i < from_parts.size(); i++) {
    if (!out.empty()) out += kSep;
    out += "..";
  }
  for (size_t i = common; i < to_parts.size(); i++) {
    if (!out.empty()) out += kSep;
    out += to_parts[i];
  }
  return out;
}

// A relative path that leaves the directory it is relative to.
bool IsOutsidePath(std::string_view rel) {
  return IsAbsolute(rel) || rel == ".." ||
         (rel.size() > 2 && rel.starts_with("..") && IsPathSeparator(rel[2]));
}

bool IsStrictDescendant(std::string_view rel) {
  return !rel.empty() && !IsOutsidePath(rel);
}

constexpr size_t kInfiniteDepth = std::numeric_limits<size_t>::max();

// How many levels a path.relative() result descends
size_t PathDepth(std::string_view rel) {
  if (rel.empty()) return 0;
  if (IsAbsolute(rel)) return kInfiniteDepth;
  return SplitSegments(rel).size();
}

// path.dirname() applied `count` times
std::string StripSegments(std::string_view fullpath, size_t count) {
  const size_t root = RootLength(fullpath);
  std::string_view body = fullpath.substr(root);
  for (size_t i = 0; i < count && !body.empty(); i++) {
    const size_t pos = body.find_last_of(kWindows ? "\\/" : "/");
    body = pos == std::string_view::npos ? std::string_view()
                                         : body.substr(0, pos);
  }
  std::string out(fullpath.substr(0, root));
  out += body;
  return out;
}

OneByteView AsOneByte(std::string_view s) {
  return OneByteView(reinterpret_cast<const char8_t*>(s.data()), s.size());
}

struct Subject {
  std::string_view utf8;
  bool ascii = true;
  PatternString wide;  // filled only when the name is not ASCII

  void Reset(std::string_view name) {
    utf8 = name;
    ascii = simdutf::validate_ascii(name.data(), name.size());
    if (!ascii) Utf8ToUtf16(name, &wide);
  }
  bool Test(const PartMatcher& part) const {
    return ascii ? TestPart(part, AsOneByte(utf8))
                 : TestPart(part, PatternView(wide));
  }
};

// Pattern#test(index, name)
bool TestIndex(const CompiledPattern::Row& row,
               size_t index,
               const Subject& name) {
  if (index >= row.parts.size()) return false;
  const PartMatcher& part = row.parts[index];
  if (part.kind == PartMatcher::Kind::kGlobstar) return true;
  return name.Test(part);
}

bool MatchFullPath(const CompiledPattern& pattern,
                   std::string_view path,
                   PatternString* scratch) {
  if (simdutf::validate_ascii(path.data(), path.size())) {
    return MatchPattern(pattern, AsOneByte(path));
  }
  Utf8ToUtf16(path, scratch);
  return MatchPattern(pattern, PatternView(*scratch));
}

struct StatInfo {
  bool exists = false;
  bool is_dir = false;
  bool is_link = false;
  int type = UV_DIRENT_UNKNOWN;
};

// The dirent type a stat's mode describes; special files keep their real
// type, as the previous implementation's lstat-built Dirents did.
int TypeOfMode(uint64_t mode) {
  switch (mode & S_IFMT) {
    case S_IFDIR:
      return UV_DIRENT_DIR;
    case S_IFLNK:
      return UV_DIRENT_LINK;
#ifdef S_IFIFO
    case S_IFIFO:
      return UV_DIRENT_FIFO;
#endif
#ifdef S_IFSOCK
    case S_IFSOCK:
      return UV_DIRENT_SOCKET;
#endif
#ifdef S_IFCHR
    case S_IFCHR:
      return UV_DIRENT_CHAR;
#endif
#ifdef S_IFBLK
    case S_IFBLK:
      return UV_DIRENT_BLOCK;
#endif
    default:
      return UV_DIRENT_FILE;
  }
}

int DirentTypeOf(const StatInfo& info) {
  if (info.type != UV_DIRENT_UNKNOWN) return info.type;
  return info.is_link  ? UV_DIRENT_LINK
         : info.is_dir ? UV_DIRENT_DIR
                       : UV_DIRENT_FILE;
}

struct DirEntry {
  std::string name;
  int type = UV_DIRENT_UNKNOWN;
};

// One blocking scandir with no Environment access, callable from any thread.
std::vector<DirEntry> ScanDir(const char* path) {
  std::vector<DirEntry> entries;
  uv_fs_t req;
  if (uv_fs_scandir(nullptr, &req, path, 0, nullptr) >= 0) {
    uv_dirent_t ent;
    while (uv_fs_scandir_next(&req, &ent) != UV_EOF) {
      entries.push_back({ent.name, ent.type});
    }
  }
  uv_fs_req_cleanup(&req);
  return entries;
}

// One blocking lstat with no Environment access, callable from any thread.
StatInfo StatRaw(const char* path) {
  StatInfo info;
  uv_fs_t req;
  if (uv_fs_lstat(nullptr, &req, path, nullptr) == 0) {
    const uint64_t mode = req.statbuf.st_mode;
    info.exists = true;
    info.is_dir = (mode & S_IFMT) == S_IFDIR;
    info.is_link = (mode & S_IFMT) == S_IFLNK;
    info.type = TypeOfMode(mode);
  }
  uv_fs_req_cleanup(&req);
  return info;
}

// LIFO scan pool
template <typename Derived>
class LifoPool {
 public:
  // Signals the threads and joins them; idempotent.
  void Stop() {
    {
      Mutex::ScopedLock lock(mutex_);
      stopping_.store(true, std::memory_order_relaxed);
      work_cond_.Broadcast(lock);
    }
    for (uv_thread_t& thread : threads_) uv_thread_join(&thread);
    threads_.clear();
  }

  bool stopping() const { return stopping_.load(std::memory_order_relaxed); }

 protected:
  void SpawnIfBacklogged(size_t backlog) {
    if (stopping()) return;
    if (threads_.size() < max_threads_ && backlog > threads_.size() + 1) {
      uv_thread_t thread;
      if (uv_thread_create(&thread, ThreadMain, this) == 0) {
        threads_.push_back(thread);
      }
    }
  }

  Mutex mutex_;
  ConditionVariable work_cond_;
  ConditionVariable done_cond_;
  // The walker itself occupies one unit of the machine's parallelism.
  const size_t max_threads_ = uv_available_parallelism() - 1;

 private:
  static void ThreadMain(void* arg) { static_cast<LifoPool*>(arg)->RunLoop(); }

  void RunLoop() {
    Derived* self = static_cast<Derived*>(this);
    Mutex::ScopedLock lock(mutex_);
    for (;;) {
      while (!self->HasRunnableJob() && !stopping()) work_cond_.Wait(lock);
      if (stopping()) return;
      self->RunOne(lock);
    }
  }

  std::atomic<bool> stopping_{false};
  std::vector<uv_thread_t> threads_;
};

class ListingPrefetcher : public LifoPool<ListingPrefetcher> {
 public:
  ~ListingPrefetcher() { Stop(); }

  // Queues `path` for a background scandir; duplicates are dropped.
  void Submit(std::string path) {
    Mutex::ScopedLock lock(mutex_);
    if (!listings_.try_emplace(path).second) return;
    jobs_.push_back(std::move(path));
    SpawnIfBacklogged(jobs_.size());
    work_cond_.Signal(lock);
  }

  // Hands out the listing for `path` when one was submitted
  bool Take(const std::string& path, std::vector<DirEntry>* out) {
    Mutex::ScopedLock lock(mutex_);
    auto it = listings_.find(path);
    if (it == listings_.end()) return false;
    // Threads insert into the map whenever the lock is released
    Listing* listing = &it->second;
    if (listing->state == State::kQueued) {
      listings_.erase(it);  // reclaimed; the job entry is skipped
      return false;
    }
    listing->wanted = true;
    while (listing->state == State::kRunning) done_cond_.Wait(lock);
    *out = std::move(listing->entries);
    listings_.erase(path);
    return true;
  }

 private:
  friend class LifoPool<ListingPrefetcher>;

  enum class State : uint8_t { kQueued, kRunning, kDone };
  struct Listing {
    State state = State::kQueued;
    bool wanted = false;
    std::vector<DirEntry> entries;
  };

  bool HasRunnableJob() const { return !jobs_.empty(); }

  void RunOne(Mutex::ScopedLock& lock) {
    // Newest first
    const std::string path = std::move(jobs_.back());
    jobs_.pop_back();
    auto it = listings_.find(path);
    if (it == listings_.end() || it->second.state != State::kQueued) {
      return;  // Reclaimed by Take().
    }
    it->second.state = State::kRunning;
    std::vector<DirEntry> entries;
    {
      Mutex::ScopedUnlock unlock(lock);
      entries = ScanDir(path.c_str());
    }
    // Submit() may have rehashed the map while the lock was dropped.
    it = listings_.find(path);
    CHECK(it != listings_.end());
    it->second.entries = std::move(entries);
    it->second.state = State::kDone;
    if (it->second.wanted) done_cond_.Broadcast(lock);
  }

  std::deque<std::string> jobs_;
  std::unordered_map<std::string, Listing> listings_;
};

// An insertion-ordered set of small indexes
class IndexSet {
 public:
  void Add(size_t v) {
    if (!Has(v)) values_.push_back(v);
  }
  bool Has(size_t v) const {
    return std::ranges::find(values_, v) != values_.end();
  }
  size_t size() const { return values_.size(); }
  bool empty() const { return values_.empty(); }
  const std::vector<size_t>& values() const { return values_; }

 private:
  std::vector<size_t> values_;
};

struct PatternState {
  const CompiledPattern::Row* row = nullptr;
  const std::vector<uint32_t>* suffix_ids = nullptr;  // per index, incl. end
  // Whether the row names an absolute path
  bool absolute = false;
  IndexSet indexes;
  IndexSet symlinks;
  // Shared because the JS clones the set only when it grows.
  std::shared_ptr<const std::vector<std::string>> realpaths;

  size_t last() const { return row->parts.size() - 1; }

  // Pattern#at() with JS negative indexing; null when out of range.
  const PartMatcher* At(ptrdiff_t index) const {
    const ptrdiff_t size = static_cast<ptrdiff_t>(row->parts.size());
    const ptrdiff_t i = index < 0 ? size + index : index;
    if (i < 0 || i >= size) return nullptr;
    return &row->parts[i];
  }

  bool IsFirst() const { return indexes.Has(0); }

  bool IsLast(bool is_directory) const {
    if (indexes.Has(last())) return true;
    const PartMatcher* tail = At(-1);
    const PartMatcher* before = At(-2);
    return tail != nullptr && tail->kind == PartMatcher::Kind::kLiteral &&
           tail->literal.empty() && is_directory && last() > 0 &&
           indexes.Has(last() - 1) && before != nullptr &&
           before->kind == PartMatcher::Kind::kGlobstar;
  }

  bool HasSeenSymlinks() const {
    for (size_t i : indexes.values()) {
      if (!symlinks.Has(i)) return true;
    }
    return false;
  }

  PatternState Child(IndexSet next_indexes,
                     IndexSet next_symlinks = IndexSet()) const {
    PatternState child;
    child.row = row;
    child.suffix_ids = suffix_ids;
    child.absolute = absolute;
    child.indexes = std::move(next_indexes);
    child.symlinks = std::move(next_symlinks);
    child.realpaths = realpaths;
    return child;
  }
};

bool IsLiteral(const PartMatcher* part) {
  return part != nullptr && part->kind == PartMatcher::Kind::kLiteral;
}

bool IsGlobstar(const PartMatcher* part) {
  return part != nullptr && part->kind == PartMatcher::Kind::kGlobstar;
}

// isAbsolute(globParts.join(sep))
bool RowIsAbsolute(const CompiledPattern::Row& row) {
  if (row.parts.empty()) return false;
  const PartMatcher& head = row.parts.front();
  if (head.kind != PartMatcher::Kind::kLiteral) return false;
  if (head.literal.empty()) return true;
  return kWindows && row.parts.size() > 1 &&
         IsWindowsDrive(PatternView(head.literal));
}

// `pattern.at(0) === '.'`
bool IsDotFirstPart(const PatternState& pattern) {
  const PartMatcher* first = pattern.At(0);
  return IsLiteral(first) && first->literal == u".";
}

class WalkerState;

// One directory visit of the free-running walk: scanned, matched, and
// emitted by the scan pool, consumed by the sequencer in schedule order.
struct FreeVisit {
  enum class State : uint8_t { kQueued, kRunning, kDone, kTaken };

  std::string rel;  // the walk-relative path results are built from
  std::string abs;
  StatInfo stat;  // from the parent's listing; the root is stat'd once
  PatternState pattern;

  std::vector<WalkEntry> results;                    // in emission order
  std::vector<std::unique_ptr<FreeVisit>> children;  // in listing order

  State state = State::kQueued;  // guarded by the pool's lock
  bool wanted = false;
};

// Threads for the free-running walk
class ScanPool : public LifoPool<ScanPool> {
 public:
  explicit ScanPool(WalkerState* walker) : walker_(walker) {}

  ~ScanPool() { Stop(); }

  void Submit(FreeVisit* visit) {
    Mutex::ScopedLock lock(mutex_);
    jobs_.push_back(visit);
    SpawnIfBacklogged(jobs_.size());
    work_cond_.Signal(lock);
  }

  void SubmitAll(const std::vector<std::unique_ptr<FreeVisit>>& children) {
    if (children.empty()) return;
    Mutex::ScopedLock lock(mutex_);
    for (const std::unique_ptr<FreeVisit>& child : children) {
      jobs_.push_back(child.get());
    }
    SpawnIfBacklogged(jobs_.size());
    if (children.size() > 1) {
      work_cond_.Broadcast(lock);
    } else {
      work_cond_.Signal(lock);
    }
  }

  // Blocks until `visit` has been scanned
  void Await(FreeVisit* visit);

 private:
  friend class LifoPool<ScanPool>;

  // Scanning runs ahead of the sequencer, so finished listings pile up.
  // High enough that the pool never starves waiting for the sequencer,
  // low enough that a huge tree cannot be held in memory all at once.
  //
  // This value can be changed as needed without it being considered a
  // breaking change, although it should be fine as it currently stands.
  static constexpr size_t kMaxReadyVisits = 1024;

  bool HasRunnableJob() const {
    return !jobs_.empty() && ready_ < kMaxReadyVisits;
  }

  void RunOne(Mutex::ScopedLock& lock);

  WalkerState* walker_;
  std::deque<FreeVisit*> jobs_;
  size_t ready_ = 0;  // finished visits the sequencer has not consumed
};

class WalkerState {
 public:
  WalkerState(Environment* env,
              const WalkOptions& options,
              const std::vector<CompiledPatternPtr>& includes,
              const std::vector<CompiledPatternPtr>& excludes)
      : env_(env), options_(options), includes_(includes), excludes_(excludes) {
    root_absolute_ = ResolveRoot(options.cwd);
    for (const CompiledPatternPtr& pattern : includes_) {
      for (const CompiledPattern::Row& row : pattern->rows) {
        rows_.push_back(&row);
        row_suffix_ids_.push_back(InternSuffixes(row));
      }
    }
    free_ = ClassifyFree();
  }

  // Advances the traversal until `max_results` new results are ready or
  // the schedule drains.
  bool RunSlice(size_t max_results,
                bool on_main_thread,
                std::vector<WalkEntry>* out) {
    on_main_thread_ = on_main_thread;
    if (!started_) {
      started_ = true;
      if (free_) {
        StartFree();
      } else {
        Start();
      }
    }
    if (free_) {
      RunFree(max_results);
    } else {
      RunSerial(max_results);
    }
    Drain(out);
    return aborted_ || pool_.stopping() ||
           (free_ ? free_stack_.empty() : queue_.empty());
  }

  // Runs one visit of the free walk; called from pool threads and from
  // the sequencer when it takes an unstarted visit.
  void FreeScan(FreeVisit* visit) {
    FreeCtx ctx{this, visit, {}};
    VisitDirectory(ctx, visit->rel, visit->pattern);
    pool_.SubmitAll(visit->children);
  }

  bool PoolStopping() const { return pool_.stopping(); }

  void StopPools() {
    pool_.Stop();
    prefetcher_.Stop();
  }

 private:
  struct QueueItem {
    std::string path;
    std::vector<PatternState> patterns;
  };

  // The free-running walk applies when every directory is provably
  // visited once and nothing can observe the traversal while it runs:
  // one pattern row, no '..' hops or absolute heads (revisits), no
  // symlink following (realpath chains), no exclude pattern or callback
  // (pruning), and no permission model (per-path checks).
  bool ClassifyFree() const {
    if (rows_.size() != 1) return false;
    if (!excludes_.empty() || options_.exclude_filter != nullptr) return false;
    if (options_.follow_symlinks) return false;
    if (env_->permission()->enabled()) return false;
    const CompiledPattern::Row& row = *rows_[0];
    if (row.parts.empty()) return false;
    for (const PartMatcher& part : row.parts) {
      if (part.kind == PartMatcher::Kind::kLiteral && part.literal == u"..") {
        return false;
      }
    }
    const PartMatcher& head = row.parts.front();
    if (head.kind == PartMatcher::Kind::kLiteral) {
      if (head.literal.empty()) return false;  // absolute
      if (kWindows && head.literal.ends_with(u':')) {
        return false;  // drive root
      }
    }
    return true;
  }

  void Start() {
    QueueItem initial;
    initial.path = ".";
    for (size_t i = 0; i < rows_.size(); i++) {
      PatternState state;
      state.row = rows_[i];
      state.suffix_ids = &row_suffix_ids_[i];
      state.absolute = RowIsAbsolute(*rows_[i]);
      state.indexes.Add(0);
      state.realpaths = std::make_shared<std::vector<std::string>>();
      initial.patterns.push_back(std::move(state));
    }
    queue_.push_back(std::move(initial));
  }

  void StartFree() {
    auto root = std::make_unique<FreeVisit>();
    root->rel = ".";
    root->abs = root_absolute_;
    root->stat = StatRaw(root->abs.c_str());
    root->pattern.row = rows_[0];
    root->pattern.suffix_ids = &row_suffix_ids_[0];
    root->pattern.absolute = RowIsAbsolute(*rows_[0]);
    root->pattern.indexes.Add(0);
    pool_.Submit(root.get());
    free_stack_.push_back(std::move(root));
  }

  void RunSerial(size_t max_results) {
    while (!aborted_ && !pool_.stopping() && !queue_.empty() &&
           results_.size() - emitted_ < max_results) {
      QueueItem item = std::move(queue_.back());
      queue_.pop_back();
      const size_t kept = queue_.size();
      SerialCtx ctx{this};
      for (PatternState& pattern : item.patterns) {
        VisitDirectory(ctx, item.path, pattern);
        if (aborted_) break;
      }
      for (std::string& entry : subpatterns_order_) {
        QueueItem next;
        next.patterns = std::move(subpatterns_[entry]);
        next.path = std::move(entry);
        queue_.push_back(std::move(next));
      }
      subpatterns_.clear();
      subpatterns_order_.clear();
      PrefetchQueued(kept);
    }
  }

  // Consumes finished visits in the exact schedule order the serial walk
  // would have produced
  void RunFree(size_t max_results) {
    while (!free_stack_.empty() && !pool_.stopping() &&
           results_.size() - emitted_ < max_results) {
      std::unique_ptr<FreeVisit> visit = std::move(free_stack_.back());
      free_stack_.pop_back();
      pool_.Await(visit.get());
      for (WalkEntry& result : visit->results) {
        if (result_set_.insert(result.path).second) {
          results_.push_back(std::move(result));
        }
      }
      for (std::unique_ptr<FreeVisit>& child : visit->children) {
        free_stack_.push_back(std::move(child));
      }
    }
  }

  void Drain(std::vector<WalkEntry>* out) {
    out->reserve(out->size() + (results_.size() - emitted_));
    for (; emitted_ < results_.size(); emitted_++) {
      out->push_back(std::move(results_[emitted_]));
    }
  }

  std::string ResolveRoot(const std::string& cwd) {
    // The only step that needs the process working directory.
    return PathResolve(env_, {cwd});
  }

  // cacheKey(index): globStrings[index..].join('/'), interned to an id so
  // the visited set stores integers instead of rebuilt strings.
  std::vector<uint32_t> InternSuffixes(const CompiledPattern::Row& row) {
    std::vector<uint32_t> ids;
    ids.reserve(row.parts.size() + 1);
    for (size_t index = 0; index <= row.parts.size(); index++) {
      std::string key;
      for (size_t i = index; i < row.parts.size(); i++) {
        key += Utf16ToUtf8(row.parts[i].source);
        if (i != row.parts.size() - 1) key += '/';
      }
      auto it = suffix_ids_.find(key);
      if (it == suffix_ids_.end()) {
        const uint32_t id = static_cast<uint32_t>(suffix_ids_.size());
        it = suffix_ids_.emplace(std::move(key), id).first;
      }
      ids.push_back(it->second);
    }
    return ids;
  }

  bool PermissionGranted(const std::string& path) {
    if (!env_->permission()->enabled()) return true;
    return env_->permission()->is_granted(
        env_, permission::PermissionScope::kFileSystemRead, path);
  }

  // Runs one libuv filesystem call. Errors are never thrown (since the
  // previous implementation didn't know)
  static bool NeverThrow(int) { return false; }

  template <typename Func, typename... Args>
  int FsCall(fs::FSReqWrapSync* req, Func fn, Args... args) {
    if (on_main_thread_) {
      return fs::SyncCallAndThrowIf(NeverThrow, env_, req, fn, args...);
    }
    return fn(nullptr, &req->req, args..., nullptr);
  }

  StatInfo StatSync(const std::string& path) {
    auto it = stat_cache_.find(path);
    if (it != stat_cache_.end()) return it->second;
    StatInfo info;
    if (PermissionGranted(path)) {
      fs::FSReqWrapSync req("lstat", path.c_str());
      if (FsCall(&req, uv_fs_lstat, path.c_str()) == 0) {
        const uint64_t mode = req.req.statbuf.st_mode;
        info.exists = true;
        info.is_dir = (mode & S_IFMT) == S_IFDIR;
        info.is_link = (mode & S_IFMT) == S_IFLNK;
        info.type = TypeOfMode(mode);
      }
    }
    stat_cache_.emplace(path, info);
    return info;
  }

  void AddToStatCache(const std::string& path, const StatInfo& info) {
    stat_cache_[path] = info;
  }

  bool FollowStatIsDirectory(const std::string& path) {
    auto it = follow_stat_cache_.find(path);
    if (it != follow_stat_cache_.end()) return it->second;
    bool is_dir = false;
    if (PermissionGranted(path)) {
      fs::FSReqWrapSync req("stat", path.c_str());
      if (FsCall(&req, uv_fs_stat, path.c_str()) == 0) {
        is_dir = (req.req.statbuf.st_mode & S_IFMT) == S_IFDIR;
      }
    }
    follow_stat_cache_.emplace(path, is_dir);
    return is_dir;
  }

  const std::string& RealpathSync(const std::string& path) {
    auto it = realpath_cache_.find(path);
    if (it != realpath_cache_.end()) return it->second;
    std::string real;
    if (PermissionGranted(path)) {
      fs::FSReqWrapSync req("realpath", path.c_str());
      if (FsCall(&req, uv_fs_realpath, path.c_str()) == 0 &&
          req.req.ptr != nullptr) {
        real = static_cast<const char*>(req.req.ptr);
      }
    }
    return realpath_cache_.emplace(path, std::move(real)).first->second;
  }

  const std::vector<DirEntry>& ReaddirSync(const std::string& path) {
    auto it = readdir_cache_.find(path);
    if (it != readdir_cache_.end()) return it->second;
    std::vector<DirEntry> entries;
    if (PermissionGranted(path)) {
      if (!prefetcher_.Take(path, &entries)) {
        fs::FSReqWrapSync req("scandir", path.c_str());
        if (FsCall(&req, uv_fs_scandir, path.c_str(), 0) >= 0) {
          uv_dirent_t ent;
          while (uv_fs_scandir_next(&req.req, &ent) != UV_EOF) {
            entries.push_back({ent.name, ent.type});
          }
        }
      }
      for (DirEntry& entry : entries) {
        if (entry.type != UV_DIRENT_UNKNOWN) continue;
        const StatInfo info = StatSync(Join(path, entry.name));
        entry.type = DirentTypeOf(info);
      }
    }
    return readdir_cache_.emplace(path, std::move(entries)).first->second;
  }

  void PrefetchQueued(size_t first_new) {
    if (aborted_) return;
    for (size_t i = first_new; i < queue_.size(); i++) {
      MaybePrefetch(queue_[i]);
    }
  }

  void MaybePrefetch(const QueueItem& item) {
    const bool needs_listing =
        std::ranges::any_of(item.patterns, [](const PatternState& pattern) {
          return pattern.indexes.size() != 1 ||
                 !IsLiteral(pattern.At(
                     static_cast<ptrdiff_t>(pattern.indexes.values()[0])));
        });
    if (!needs_listing) return;
    std::string fullpath = Resolve(root_absolute_, item.path);
    if (readdir_cache_.contains(fullpath)) return;
    auto cached = stat_cache_.find(fullpath);
    if (cached != stat_cache_.end() && !cached->second.is_dir &&
        !cached->second.is_link) {
      return;
    }
    if (!PermissionGranted(fullpath)) return;
    prefetcher_.Submit(std::move(fullpath));
  }

  // Cache#add
  bool MarkSeen(const std::string& path, const PatternState& pattern) {
    std::unordered_set<uint32_t>& keys = seen_[path];
    const size_t original = keys.size();
    for (size_t index : pattern.indexes.values()) {
      const size_t clamped = std::min(index, pattern.row->parts.size());
      keys.insert((*pattern.suffix_ids)[clamped]);
    }
    return keys.size() != original + pattern.indexes.size();
  }

  bool Seen(const std::string& path,
            const PatternState& pattern,
            size_t index) const {
    auto it = seen_.find(path);
    if (it == seen_.end()) return false;
    const size_t clamped = std::min(index, pattern.row->parts.size());
    return it->second.contains((*pattern.suffix_ids)[clamped]);
  }

  // options.maxDepthh
  bool HasDepthLimit() const {
    return options_.max_depth != WalkOptions::kNoDepthLimit;
  }

  // The depth of a walk path that never leaves cwd
  static std::optional<size_t> PlainDepth(const std::string& path) {
    if (path == ".") return 0;
    if (IsAbsolute(path)) return std::nullopt;
    size_t depth = 0;
    for (std::string_view segment : SplitSegments(path)) {
      if (segment == "..") return std::nullopt;
      if (segment != ".") depth++;
    }
    return depth;
  }

  size_t DepthFromRoot(const std::string& path) {
    if (const std::optional<size_t> plain = PlainDepth(path)) return *plain;
    CHECK(!free_);
    auto it = depth_cache_.find(path);
    if (it != depth_cache_.end()) return it->second;
    const std::string fullpath = Resolve(root_absolute_, path);
    std::string rel = Relative(root_absolute_, fullpath);
    if (kWindows) {
      // path.relative() folds case, so `rel` may descend from a directory
      // that merely spells cwd differently; only the same directory counts.
      if (!IsOutsidePath(rel)) {
        const std::string variant_root =
            StripSegments(fullpath, PathDepth(rel));
        if (!SamePath(root_absolute_, variant_root)) {
          rel = Relative(root_absolute_, fullpath, /*case_sensitive=*/true);
        }
      }
    } else if (kMacOS) {
      // The reverse: a case-insensitive filesystem makes a differently
      // spelled cwd the same directory, which a case-sensitive
      // path.relative() reports as outside.
      if (!IsAbsolute(rel) && IsOutsidePath(rel)) {
        const std::string folded =
            Relative(Lower(root_absolute_), Lower(fullpath));
        if (!IsOutsidePath(folded)) {
          const std::string variant_root =
              StripSegments(fullpath, PathDepth(folded));
          if (SamePath(root_absolute_, variant_root)) rel = folded;
        }
      }
    }
    const size_t depth = PathDepth(rel);
    depth_cache_.emplace(path, depth);
    return depth;
  }

  // Whether two spellings name one directory.
  bool SamePath(const std::string& a, const std::string& b) {
    if (a == b) return true;
    if (kWindows) {
      const std::string real = RealpathSync(a);
      return !real.empty() && real == RealpathSync(b);
    } else {
      const std::optional<Identity> identity = IdentityOf(a);
      return identity.has_value() && identity == IdentityOf(b);
    }
  }

  using Identity = std::pair<uint64_t, uint64_t>;  // (st_dev, st_ino)

  std::optional<Identity> IdentityOf(const std::string& path) {
    auto it = identity_cache_.find(path);
    if (it != identity_cache_.end()) return it->second;
    std::optional<Identity> identity;
    if (PermissionGranted(path)) {
      fs::FSReqWrapSync req("lstat", path.c_str());
      if (FsCall(&req, uv_fs_lstat, path.c_str()) == 0) {
        identity = Identity{req.req.statbuf.st_dev, req.req.statbuf.st_ino};
      }
    }
    identity_cache_.emplace(path, identity);
    return identity;
  }

  // Whether `path` is a directory above cwd, which an absolute pattern
  // has to pass through on its way down.
  bool IsAncestorOfRoot(const std::string& path) {
    CHECK(!free_);
    auto it = ancestor_cache_.find(path);
    if (it != ancestor_cache_.end()) return it->second;
    const std::string fullpath = Resolve(root_absolute_, path);
    bool candidate = IsStrictDescendant(Relative(fullpath, root_absolute_));
    bool needs_identity = kWindows;
    if (kMacOS && !candidate) {
      candidate =
          IsStrictDescendant(Relative(Lower(fullpath), Lower(root_absolute_)));
      needs_identity = true;
    }
    bool result = candidate;
    if (candidate && needs_identity) {
      const std::string real_root = RealpathSync(root_absolute_);
      const std::string real_full = RealpathSync(fullpath);
      result = !real_root.empty() && !real_full.empty() &&
               IsStrictDescendant(
                   Relative(real_full, real_root, /*case_sensitive=*/true));
    }
    ancestor_cache_.emplace(path, result);
    return result;
  }

  bool WithinDepth(const std::string& path) {
    return !HasDepthLimit() || DepthFromRoot(path) <= options_.max_depth;
  }

  bool MayDescend(const std::string& path, const PatternState& pattern) {
    if (!HasDepthLimit()) return true;
    if (pattern.absolute && IsAncestorOfRoot(path)) return true;
    return DepthFromRoot(path) < options_.max_depth;
  }

  bool IsExcluded(std::string_view path) {
    for (const CompiledPatternPtr& pattern : excludes_) {
      if (MatchFullPath(*pattern, path, &scratch_)) return true;
    }
    return false;
  }

  void AddResult(std::string path, int type) {
    if (!WithinDepth(path)) return;
    if (!excludes_.empty() && IsExcluded(Resolve(root_absolute_, path))) {
      return;
    }
    if (result_set_.insert(path).second) {
      results_.push_back({std::move(path), type});
    }
  }

  void AddSubpattern(const std::string& path, PatternState pattern) {
    if (!excludes_.empty()) {
      if (IsExcluded(path)) return;
      const std::string full = Resolve(root_absolute_, path);
      if (IsExcluded(full + '/') && StatSync(full).is_dir) return;
    }
    if (options_.exclude_filter != nullptr) {
      const bool excluded = options_.exclude_filter->ExcludesPath(path);
      if (options_.exclude_filter->failed()) {
        aborted_ = true;
        return;
      }
      if (excluded) return;
    }
    auto it = subpatterns_.find(path);
    if (it == subpatterns_.end()) {
      subpatterns_.emplace(path, std::vector<PatternState>{std::move(pattern)});
      subpatterns_order_.push_back(path);
    } else {
      it->second.push_back(std::move(pattern));
    }
  }

  int TypeOfResolved(const std::string& path) {
    return DirentTypeOf(StatSync(Resolve(root_absolute_, path)));
  }

  // The serial (guarded) walk: filesystem access is cached, traced, and
  // permission-checked; the seen/'..' machinery is live.
  struct SerialCtx {
    static constexpr bool kSerial = true;
    WalkerState* w;

    bool follow() const { return w->options_.follow_symlinks; }
    bool Aborted() const { return w->aborted_; }
    bool HasFilter() const { return w->options_.exclude_filter != nullptr; }
    std::string FullPathFor(const std::string& path) const {
      return Resolve(w->root_absolute_, path);
    }
    bool MarkSeenVisit(const std::string& path, const PatternState& pattern) {
      return w->MarkSeen(path, pattern);
    }
    StatInfo VisitStat(const std::string& fullpath) {
      return w->StatSync(fullpath);
    }
    bool ExcludedFull(const std::string& fullpath) {
      return !w->excludes_.empty() && w->IsExcluded(fullpath);
    }
    StatInfo ProbeStat(const std::string& fullpath) {
      return w->StatSync(fullpath);
    }
    const std::vector<DirEntry>* Listing(const std::string& fullpath) {
      return &w->ReaddirSync(fullpath);
    }
    bool FilterEntry(const std::string& name,
                     const std::string& parent,
                     int type) {
      const bool excluded =
          w->options_.exclude_filter->ExcludesEntry(name, parent, type);
      if (w->options_.exclude_filter->failed()) w->aborted_ = true;
      return excluded;
    }
    void NoteChildStat(const std::string& fullpath, const StatInfo& info) {
      w->AddToStatCache(fullpath, info);
    }
    bool FollowIsDir(const std::string& fullpath) {
      return w->FollowStatIsDirectory(fullpath);
    }
    void ResultAdd(std::string path, int type) {
      w->AddResult(std::move(path), type);
    }
    void AddChildEntry(std::string path,
                       PatternState state,
                       std::string abs,
                       const StatInfo& stat,
                       const std::string& name) {
      w->AddSubpattern(path, std::move(state));
    }
    void AddChildRedirect(std::string path, PatternState state) {
      w->AddSubpattern(path, std::move(state));
    }
  };

  // The free-running walk: every syscall is raw (nothing can observe it),
  // nothing is cached (every directory is visited once), and results and
  // children collect on the visit for the sequencer.
  struct FreeCtx {
    static constexpr bool kSerial = false;
    WalkerState* w;
    FreeVisit* v;
    std::vector<DirEntry> listing;

    bool follow() const { return false; }
    bool Aborted() const { return w->PoolStopping(); }
    bool HasFilter() const { return false; }
    const std::string& FullPathFor(const std::string& path) const {
      return v->abs;
    }
    bool MarkSeenVisit(const std::string&, const PatternState&) {
      return false;
    }
    StatInfo VisitStat(const std::string&) const { return v->stat; }
    bool ExcludedFull(const std::string&) const { return false; }
    StatInfo ProbeStat(const std::string& fullpath) {
      return StatRaw(fullpath.c_str());
    }
    const std::vector<DirEntry>* Listing(const std::string& fullpath) {
      listing = ScanDir(fullpath.c_str());
      for (DirEntry& entry : listing) {
        if (entry.type != UV_DIRENT_UNKNOWN) continue;
        entry.type =
            DirentTypeOf(StatRaw(JoinEntry(fullpath, entry.name).c_str()));
      }
      return &listing;
    }
    bool FilterEntry(const std::string&, const std::string&, int) {
      return false;
    }
    void NoteChildStat(const std::string&, const StatInfo&) {}
    bool FollowIsDir(const std::string&) { return false; }
    void ResultAdd(std::string path, int type) {
      if (!w->WithinDepth(path)) return;
      v->results.push_back({std::move(path), type});
    }
    void AddChildEntry(std::string path,
                       PatternState state,
                       std::string abs,
                       const StatInfo& stat,
                       const std::string& name) {
      auto child = std::make_unique<FreeVisit>();
      child->rel = std::move(path);
      child->abs = abs.empty() ? JoinEntry(v->abs, name) : std::move(abs);
      child->stat = stat;
      child->pattern = std::move(state);
      v->children.push_back(std::move(child));
    }
    void AddChildRedirect(std::string path, PatternState state) {
      // Only the leading-'.' rewrite is classified free; it stays on the
      // same directory.
      CHECK(path == v->rel);
      auto child = std::make_unique<FreeVisit>();
      child->rel = std::move(path);
      child->abs = v->abs;
      child->stat = v->stat;
      child->pattern = std::move(state);
      v->children.push_back(std::move(child));
    }
  };

  // One directory visit: the traversal semantics of the previous JS
  // implementation, shared verbatim between the serial and free walks
  // through the context above.
  template <typename Ctx>
  void VisitDirectory(Ctx& ctx,
                      const std::string& path,
                      PatternState& pattern) {
    if (ctx.MarkSeenVisit(path, pattern)) return;

    const std::string& fullpath = ctx.FullPathFor(path);
    const StatInfo stat = ctx.VisitStat(fullpath);
    const size_t last = pattern.last();
    bool is_directory = stat.is_dir;
    if (!is_directory && stat.is_link) {
      is_directory =
          ctx.follow() ? ctx.FollowIsDir(fullpath) : pattern.HasSeenSymlinks();
    }
    const bool is_last = pattern.IsLast(is_directory);
    const bool is_first = pattern.IsFirst();

    if (ctx.ExcludedFull(fullpath)) return;

    const PartMatcher* first_part = pattern.At(0);
    if (is_first && kWindows && IsLiteral(first_part) &&
        first_part->literal.ends_with(u':')) {
      // Absolute path, go to root
      IndexSet next;
      next.Add(1);
      ctx.AddChildRedirect(Utf16ToUtf8(first_part->literal) + "\\",
                           pattern.Child(std::move(next)));
      return;
    }
    if (is_first && IsLiteral(first_part) && first_part->literal.empty()) {
      IndexSet next;
      next.Add(1);
      ctx.AddChildRedirect("/", pattern.Child(std::move(next)));
      return;
    }
    if (is_first && IsLiteral(first_part) && first_part->literal == u"..") {
      IndexSet next;
      next.Add(1);
      ctx.AddChildRedirect("../", pattern.Child(std::move(next)));
      return;
    }
    if (is_first && IsLiteral(first_part) && first_part->literal == u".") {
      IndexSet next;
      next.Add(1);
      ctx.AddChildRedirect(".", pattern.Child(std::move(next)));
      return;
    }

    const PartMatcher* tail = pattern.At(-1);
    if (is_last && IsLiteral(tail)) {
      const std::string p = Utf16ToUtf8(tail->literal);
      const StatInfo tail_stat = ctx.ProbeStat(Join(fullpath, p));
      if (tail_stat.exists && (!p.empty() || is_directory)) {
        ctx.ResultAdd(Join(path, p), DirentTypeOf(tail_stat));
      }
      if (pattern.indexes.size() == 1 && pattern.indexes.Has(last)) return;
    } else if (is_last && IsGlobstar(tail) &&
               (path != "." || IsDotFirstPart(pattern) ||
                (last == 0 && stat.exists))) {
      // A pattern ending in ** returns the directory itself, except for
      // "." unless the pattern starts with "." or is exactly "**".
      ctx.ResultAdd(path, DirentTypeOf(stat));
    }

    if (!is_directory || !MayDescend(path, pattern)) return;

    [[maybe_unused]] std::shared_ptr<const std::vector<std::string>>
        next_realpaths;
    if constexpr (Ctx::kSerial) {
      if (IsCyclic(fullpath, is_directory, pattern)) return;
      next_realpaths = NextRealpaths(fullpath, is_directory, pattern);
    }

    // When exactly one literal position is live, probe that child
    // directly instead of enumerating the directory.
    std::vector<DirEntry> literal_child;
    const std::vector<DirEntry>* children = nullptr;
    if (pattern.indexes.size() == 1) {
      const PartMatcher* only =
          pattern.At(static_cast<ptrdiff_t>(pattern.indexes.values()[0]));
      if (IsLiteral(only)) {
        std::string cname = Utf16ToUtf8(only->literal);
        const StatInfo info = ctx.ProbeStat(Join(fullpath, cname));
        if (!info.exists) return;
        literal_child.push_back({std::move(cname), DirentTypeOf(info)});
        children = &literal_child;
      }
    }
    if (children == nullptr) children = ctx.Listing(fullpath);

    Subject name;
    for (const DirEntry& entry : *children) {
      if (ctx.Aborted()) return;
      const std::string entry_path = JoinEntry(path, entry.name);
      StatInfo entry_stat;
      entry_stat.exists = true;
      entry_stat.is_dir = entry.type == UV_DIRENT_DIR;
      entry_stat.is_link = entry.type == UV_DIRENT_LINK;
      entry_stat.type = entry.type;

      // The absolute path is only needed for entries that can be
      // descended into
      std::string entry_fullpath;
      bool entry_is_directory = entry_stat.is_dir;
      if (entry_stat.is_dir || entry_stat.is_link) {
        entry_fullpath = JoinEntry(fullpath, entry.name);
        ctx.NoteChildStat(entry_fullpath, entry_stat);
        if (!entry_is_directory && ctx.follow()) {
          entry_is_directory = ctx.FollowIsDir(entry_fullpath);
        }
      }

      name.Reset(entry.name);

      IndexSet sub_patterns;
      IndexSet next_symlinks;
      for (size_t index : pattern.indexes.values()) {
        const PartMatcher* current = pattern.At(static_cast<ptrdiff_t>(index));
        const size_t next_index = index + 1;
        const PartMatcher* next =
            pattern.At(static_cast<ptrdiff_t>(next_index));
        const bool from_symlink = !ctx.follow() && pattern.symlinks.Has(index);

        if (IsGlobstar(current)) {
          const bool is_dot = !entry.name.empty() && entry.name[0] == '.';
          const bool next_matches = TestIndex(*pattern.row, next_index, name);

          size_t next_non_glob = next_index;
          while (
              IsGlobstar(pattern.At(static_cast<ptrdiff_t>(next_non_glob)))) {
            next_non_glob++;
          }
          const bool matches_dot =
              is_dot && TestIndex(*pattern.row, next_non_glob, name);

          if (is_dot && !matches_dot) continue;
          if (ctx.HasFilter()) {
            const bool excluded =
                ctx.FilterEntry(entry.name, fullpath, entry.type);
            if (ctx.Aborted()) return;
            if (excluded) continue;
          }

          if (!from_symlink && entry_is_directory) {
            sub_patterns.Add(index);
          } else if (!from_symlink && index == last) {
            ctx.ResultAdd(entry_path, entry.type);
          }

          if (next_matches && next_index == last && !is_last) {
            ctx.ResultAdd(entry_path, entry.type);
          } else if (next_matches && entry_is_directory) {
            sub_patterns.Add(index + 2);
          }
          if ((next_matches || IsDotFirstPart(pattern)) && entry_is_directory &&
              !from_symlink) {
            sub_patterns.Add(next_index);
          }

          if (!ctx.follow() && entry_stat.is_link) {
            next_symlinks.Add(index);
          }

          if constexpr (Ctx::kSerial) {
            if (IsLiteral(next) && next->literal == u".." &&
                entry_is_directory) {
              // "**/..": both this directory and its parent stay live.
              const std::string parent = Join(path, "..");
              if (next_index < last) {
                IndexSet forward;
                forward.Add(next_index + 1);
                if (!subpatterns_.contains(path) &&
                    !Seen(path, pattern, next_index + 1)) {
                  subpatterns_.emplace(
                      path, std::vector<PatternState>{pattern.Child(forward)});
                  subpatterns_order_.push_back(path);
                }
                if (!subpatterns_.contains(parent) &&
                    !Seen(parent, pattern, next_index + 1)) {
                  subpatterns_.emplace(
                      parent,
                      std::vector<PatternState>{pattern.Child(forward)});
                  subpatterns_order_.push_back(parent);
                }
              } else {
                IndexSet at_next;
                at_next.Add(next_index);
                if (!Seen(path, pattern, next_index)) {
                  PatternState child = pattern.Child(at_next);
                  MarkSeen(path, child);
                  AddResult(path, DirentTypeOf(stat));
                }
                if (!Seen(path, pattern, next_index) ||
                    !Seen(parent, pattern, next_index)) {
                  PatternState child = pattern.Child(at_next);
                  MarkSeen(parent, child);
                  AddResult(parent, TypeOfResolved(parent));
                }
              }
            }
          }
        }
        if (IsLiteral(current)) {
          if (TestIndex(*pattern.row, index, name) && index != last) {
            sub_patterns.Add(next_index);
          } else if (current->literal == u"." &&
                     TestIndex(*pattern.row, next_index, name)) {
            if (next_index == last) {
              ctx.ResultAdd(entry_path, entry.type);
            } else {
              sub_patterns.Add(next_index + 1);
            }
          }
        }
        if (current != nullptr &&
            current->kind == PartMatcher::Kind::kProgram &&
            TestIndex(*pattern.row, index, name)) {
          if (index == last) {
            ctx.ResultAdd(entry_path, entry.type);
          } else if (entry_is_directory) {
            sub_patterns.Add(next_index);
          }
        }
      }
      if (!sub_patterns.empty()) {
        PatternState child =
            pattern.Child(std::move(sub_patterns), std::move(next_symlinks));
        if constexpr (Ctx::kSerial) child.realpaths = next_realpaths;
        ctx.AddChildEntry(entry_path,
                          std::move(child),
                          std::move(entry_fullpath),
                          entry_stat,
                          entry.name);
      }
    }
  }

  bool IsCyclic(const std::string& path,
                bool is_directory,
                const PatternState& pattern) {
    if (!options_.follow_symlinks || !is_directory) return false;
    const std::string& real = RealpathSync(path);
    if (real.empty()) return false;
    return std::ranges::find(*pattern.realpaths, real) !=
           pattern.realpaths->end();
  }

  std::shared_ptr<const std::vector<std::string>> NextRealpaths(
      const std::string& path, bool is_directory, const PatternState& pattern) {
    if (!options_.follow_symlinks || !is_directory) return pattern.realpaths;
    const std::string& real = RealpathSync(path);
    if (real.empty()) return pattern.realpaths;
    auto next = std::make_shared<std::vector<std::string>>(*pattern.realpaths);
    if (std::ranges::find(*next, real) == next->end()) {
      next->push_back(real);
    }
    return next;
  }

  Environment* env_;

  const WalkOptions options_;
  const std::vector<CompiledPatternPtr> includes_;
  const std::vector<CompiledPatternPtr> excludes_;
  std::string root_absolute_;

  // Rows of the include patterns
  std::vector<const CompiledPattern::Row*> rows_;
  std::vector<std::vector<uint32_t>> row_suffix_ids_;
  std::unordered_map<std::string, uint32_t> suffix_ids_;

  bool free_ = false;

  // Serial (guarded) walk state.
  std::vector<QueueItem> queue_;
  std::unordered_map<std::string, std::vector<PatternState>> subpatterns_;
  std::vector<std::string> subpatterns_order_;
  ListingPrefetcher prefetcher_;
  std::unordered_map<std::string, std::unordered_set<uint32_t>> seen_;
  std::unordered_map<std::string, StatInfo> stat_cache_;
  std::unordered_map<std::string, bool> follow_stat_cache_;
  std::unordered_map<std::string, std::string> realpath_cache_;
  std::unordered_map<std::string, std::vector<DirEntry>> readdir_cache_;
  // Depth limit state, for paths that leave cwd
  std::unordered_map<std::string, size_t> depth_cache_;
  std::unordered_map<std::string, std::optional<Identity>> identity_cache_;
  std::unordered_map<std::string, bool> ancestor_cache_;

  // Free walk state. The stack owns the visits and is declared before
  // the pool so the pool's threads are joined before any visit is freed.
  std::vector<std::unique_ptr<FreeVisit>> free_stack_;
  ScanPool pool_{this};

  std::vector<WalkEntry> results_;
  std::unordered_set<std::string> result_set_;
  size_t emitted_ = 0;
  bool started_ = false;
  bool on_main_thread_ = true;

  // Set when an exclude callback threw
  bool aborted_ = false;
  PatternString scratch_;
};

void ScanPool::Await(FreeVisit* visit) {
  Mutex::ScopedLock lock(mutex_);
  if (visit->state == FreeVisit::State::kQueued) {
    visit->state = FreeVisit::State::kTaken;
    // The job entry would dangle once the sequencer frees the visit.
    auto it = std::ranges::find(jobs_, visit);
    if (it != jobs_.end()) jobs_.erase(it);
    Mutex::ScopedUnlock unlock(lock);
    walker_->FreeScan(visit);
    return;
  }
  visit->wanted = true;
  while (visit->state != FreeVisit::State::kDone) done_cond_.Wait(lock);
  ready_--;
  work_cond_.Signal(lock);  // room for another finished listing
}

void ScanPool::RunOne(Mutex::ScopedLock& lock) {
  FreeVisit* visit = jobs_.back();
  jobs_.pop_back();
  if (visit->state != FreeVisit::State::kQueued) return;
  visit->state = FreeVisit::State::kRunning;
  {
    Mutex::ScopedUnlock unlock(lock);
    walker_->FreeScan(visit);
  }
  visit->state = FreeVisit::State::kDone;
  ready_++;
  if (visit->wanted) done_cond_.Broadcast(lock);
}

}  // namespace

// The pimpl the public Walk hands its calls to.
class WalkerImpl : public WalkerState {
 public:
  using WalkerState::WalkerState;
};

Walk::Walk(Environment* env,
           const WalkOptions& options,
           const std::vector<CompiledPatternPtr>& includes,
           const std::vector<CompiledPatternPtr>& excludes)
    : impl_(std::make_unique<WalkerImpl>(env, options, includes, excludes)) {}

Walk::~Walk() = default;

bool Walk::RunSlice(size_t max_results,
                    bool on_main_thread,
                    std::vector<WalkEntry>* out) {
  return impl_->RunSlice(max_results, on_main_thread, out);
}

void Walk::Stop() {
  impl_->StopPools();
}

void GlobSync(Environment* env,
              const WalkOptions& options,
              const std::vector<CompiledPatternPtr>& includes,
              const std::vector<CompiledPatternPtr>& excludes,
              std::vector<WalkEntry>* out) {
  Walk walk(env, options, includes, excludes);
  while (!walk.RunSlice(std::numeric_limits<size_t>::max(),
                        /*on_main_thread=*/true,
                        out)) {
  }
}

}  // namespace node::glob
