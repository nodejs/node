#include "compile_cache.h"
#include <array>
#include <memory>
#include <string>
#include "debug_utils-inl.h"
#include "env-inl.h"
#include "node_file.h"
#include "node_internals.h"
#include "node_version.h"
#include "path.h"
#include "util.h"
#include "zlib.h"
#include "zstd.h"
// kCompileCacheZstdDict + kCompileCacheZstdDictSize come from the header
// generated at build time by the GYP action (from src/compile_cache_zstd.dict).
// The include directory (SHARED_INTERMEDIATE_DIR) is added by node.gyp.
#include "compile_cache_zstd_dict.h"

#ifdef NODE_IMPLEMENTS_POSIX_CREDENTIALS
#include <unistd.h>  // getuid
#endif

#ifdef _WIN32
#include <windows.h>
#endif
namespace node {

using v8::Function;
using v8::Local;
using v8::Module;
using v8::ScriptCompiler;
using v8::String;

namespace {
// The compile-cache zstd dictionary is immutable and embedded in the binary,
// so the prepared CDict/DDict are created once and shared across all handlers
// (and all Environments/Workers) instead of per handler. They live for the
// lifetime of the process. Returns nullptr if preparation fails, in which
// case callers fall back to plain (dictionary-less) zstd.
ZSTD_CDict* GetCompileCacheCDict() {
  static ZSTD_CDict* cdict =
      ZSTD_createCDict(kCompileCacheZstdDict, kCompileCacheZstdDictSize, 1);
  return cdict;
}

ZSTD_DDict* GetCompileCacheDDict() {
  static ZSTD_DDict* ddict =
      ZSTD_createDDict(kCompileCacheZstdDict, kCompileCacheZstdDictSize);
  return ddict;
}

// The dictionary only helps small/medium caches; for larger inputs zstd's own
// adaptive model dominates and the dictionary never wins, so we skip the
// (otherwise wasted) second compression above this raw size. Decompression is
// unaffected: a single DDict decodes both dict-assisted and plain frames.
constexpr uint32_t kCompileCacheDictMaxRawSize = 256 * 1024;

std::string Uint32ToHex(uint32_t crc) {
  std::string str;
  str.reserve(8);

  for (int i = 28; i >= 0; i -= 4) {
    char digit = (crc >> i) & 0xF;
    digit += digit < 10 ? '0' : 'a' - 10;
    str.push_back(digit);
  }

  return str;
}

// TODO(joyeecheung): use other hashes?
uint32_t GetHash(const char* data, size_t size) {
  uLong crc = crc32(0L, Z_NULL, 0);
  return crc32(crc, reinterpret_cast<const Bytef*>(data), size);
}

std::string GetCacheVersionTag() {
  // On platforms where uids are available, use different folders for
  // different users to avoid cache miss due to permission incompatibility.
  // On platforms where uids are not available, bare with the cache miss.
  // This should be fine on Windows, as there local directories tend to be
  // user-specific.
  std::string tag = std::string(NODE_VERSION) + '-' + std::string(NODE_ARCH) +
                    '-' + Uint32ToHex(ScriptCompiler::CachedDataVersionTag());
#ifdef NODE_IMPLEMENTS_POSIX_CREDENTIALS
  tag += '-' + std::to_string(getuid());
#endif
  return tag;
}

uint32_t GetCacheKey(std::string_view filename, CachedCodeType type) {
  uLong crc = crc32(0L, Z_NULL, 0);
  crc = crc32(crc, reinterpret_cast<const Bytef*>(&type), sizeof(type));
  crc = crc32(
      crc, reinterpret_cast<const Bytef*>(filename.data()), filename.length());
  return crc;
}
}  // namespace

template <typename... Args>
inline void CompileCacheHandler::Debug(const char* format,
                                       Args&&... args) const {
  if (is_debug_) [[unlikely]] {
    FPrintF(stderr, format, std::forward<Args>(args)...);
  }
}

ScriptCompiler::CachedData* CompileCacheEntry::WrapCache() const {
  DCHECK_NOT_NULL(cache);
  // The returned CachedData does not own the buffer - it's a view into
  // the buffer owned by this entry, which outlives the synchronous
  // consumption of the cache during compilation, so no copy is necessary.
  return new ScriptCompiler::CachedData(
      cache->data, cache->length, ScriptCompiler::CachedData::BufferNotOwned);
}

// Used for identifying and verifying a file is a compile cache file.
// See comments in CompileCacheHandler::Persist().
// The last byte is bumped whenever the format of the cache file changes
// so that files in an older format are discarded as cache misses and
// then overwritten with the new format.
constexpr uint32_t kCacheMagicNumber = 0x8adfdbb3;

const char* CompileCacheEntry::type_name() const {
  switch (type) {
    case CachedCodeType::kCommonJS:
      return "CommonJS";
    case CachedCodeType::kESM:
      return "ESM";
    case CachedCodeType::kStrippedTypeScript:
      return "StrippedTypeScript";
    default:
      UNREACHABLE();
  }
}

void CompileCacheHandler::ReadCacheFile(CompileCacheEntry* entry) {
  Debug("[compile cache] reading cache from %s for %s %s...",
        entry->cache_filename,
        entry->type_name(),
        entry->source_filename);

  uv_fs_t req;
  auto defer_req_cleanup = OnScopeLeave([&req]() { uv_fs_req_cleanup(&req); });
  const char* path = entry->cache_filename.c_str();
  uv_file file = uv_fs_open(nullptr, &req, path, O_RDONLY, 0, nullptr);
  if (req.result < 0) {
    // req will be cleaned up by scope leave.
    Debug(" %s\n", uv_strerror(req.result));
    return;
  }
  uv_fs_req_cleanup(&req);

  auto defer_close = OnScopeLeave([file]() {
    uv_fs_t close_req;
    CHECK_EQ(0, uv_fs_close(nullptr, &close_req, file, nullptr));
    uv_fs_req_cleanup(&close_req);
  });

  // Get the file size upfront so that the cache can be read with a single
  // exactly-sized read, and truncated or trailing data can be detected
  // without additional read attempts.
  int err = uv_fs_fstat(nullptr, &req, file, nullptr);
  if (err < 0) {
    Debug("fstat failed, %s\n", uv_strerror(err));
    return;
  }
  uint64_t file_size = req.statbuf.st_size;
  uv_fs_req_cleanup(&req);

  // Read the headers.
  std::array<uint32_t, kHeaderCount> headers;
  uv_buf_t headers_buf =
      uv_buf_init(reinterpret_cast<char*>(headers.data()), kHeaderSize);
  const int r = uv_fs_read(nullptr, &req, file, &headers_buf, 1, 0, nullptr);
  if (r != static_cast<int>(headers_buf.len)) {
    Debug("reading header failed, bytes read %d", r);
    if (req.result < 0 && is_debug_) {
      Debug(", %s", uv_strerror(req.result));
    }
    Debug("\n");
    return;
  }
  uv_fs_req_cleanup(&req);

  Debug("[%d %d %d %d %d %d]...",
        headers[kMagicNumberOffset],
        headers[kCodeSizeOffset],
        headers[kCacheSizeOffset],
        headers[kCodeHashOffset],
        headers[kCacheHashOffset],
        headers[kCacheRawSizeOffset]);

  if (headers[kMagicNumberOffset] != kCacheMagicNumber) {
    Debug("magic number mismatch: expected %d, actual %d\n",
          kCacheMagicNumber,
          headers[kMagicNumberOffset]);
    return;
  }

  // Check the code size and hash which are already computed.
  if (headers[kCodeSizeOffset] != entry->code_size) {
    Debug("code size mismatch: expected %d, actual %d\n",
          entry->code_size,
          headers[kCodeSizeOffset]);
    return;
  }
  if (headers[kCodeHashOffset] != entry->code_hash) {
    Debug("code hash mismatch: expected %d, actual %d\n",
          entry->code_hash,
          headers[kCodeHashOffset]);
    return;
  }

  uint32_t cache_size = headers[kCacheSizeOffset];
  uint32_t raw_size = headers[kCacheRawSizeOffset];

  // Check the cache size. The headers were read successfully, so
  // file_size >= kHeaderSize here. The file must contain exactly the
  // headers followed by cache_size bytes of cache content.
  if (file_size - kHeaderSize != cache_size) {
    Debug("cache size mismatch: expected %d, actual %d\n",
          cache_size,
          file_size - kHeaderSize);
    return;
  }

  // The cache content is stored uncompressed when cache_size == raw_size,
  // and zstd-compressed when cache_size < raw_size (see
  // CompileCacheHandler::Persist()). Anything else is invalid.
  if (cache_size > raw_size) {
    Debug(
        "invalid cache size %d > uncompressed size %d\n", cache_size, raw_size);
    return;
  }

  // Read the cache content in one go with an exactly-sized buffer,
  // looping only in case of short reads.
  std::unique_ptr<uint8_t[]> disk_data(new uint8_t[cache_size]);
  size_t total_read = 0;
  while (total_read < cache_size) {
    uv_buf_t iov =
        uv_buf_init(reinterpret_cast<char*>(disk_data.get() + total_read),
                    cache_size - total_read);
    int bytes_read = uv_fs_read(
        nullptr, &req, file, &iov, 1, kHeaderSize + total_read, nullptr);
    if (req.result < 0) {  // Error.
      // req will be cleaned up by scope leave.
      Debug(" %s\n", uv_strerror(req.result));
      return;
    }
    uv_fs_req_cleanup(&req);
    if (bytes_read == 0) {  // Unexpected EOF - the file shrank under us.
      Debug("cache size mismatch: expected %d, actual %d\n",
            cache_size,
            total_read);
      return;
    }
    total_read += bytes_read;
  }

  // Check the cache hash of the on-disk content before decompressing.
  uint32_t cache_hash =
      GetHash(reinterpret_cast<char*>(disk_data.get()), cache_size);
  if (headers[kCacheHashOffset] != cache_hash) {
    Debug("cache hash mismatch: expected %d, actual %d\n",
          headers[kCacheHashOffset],
          cache_hash);
    return;
  }

  if (cache_size == raw_size) {
    // Stored uncompressed - hand the buffer to V8 directly.
    entry->cache.reset(new ScriptCompiler::CachedData(
        disk_data.release(),
        raw_size,
        ScriptCompiler::CachedData::BufferOwned));
  } else {
    // Cross-check the content size embedded in the zstd frame before
    // allocating, in case the headers are corrupted.
    unsigned long long content_size =  // NOLINT(runtime/int)
        ZSTD_getFrameContentSize(disk_data.get(), cache_size);
    if (content_size != raw_size) {
      Debug("uncompressed size mismatch: expected %d, actual %d\n",
            raw_size,
            content_size);
      return;
    }
    // Lazily create the decompression context on first use and reuse it
    // for subsequent reads - recreating its workspace for every file
    // costs more than the decompression itself for small caches.
    if (zstd_dctx_ == nullptr && (zstd_dctx_ = ZSTD_createDCtx()) == nullptr) {
      Debug("failed to create zstd context\n");
      return;
    }
    // Decompress directly into the buffer handed to V8. The embedded
    // dictionary is referenced via a shared, prepared DDict; plain frames
    // (which carry no dictID) decompress correctly with it as well.
    std::unique_ptr<uint8_t[]> raw_data(new uint8_t[raw_size]);
    ZSTD_DDict* ddict = GetCompileCacheDDict();
    size_t decompressed_size;
    if (ddict != nullptr) {
      decompressed_size = ZSTD_decompress_usingDDict(
          zstd_dctx_, raw_data.get(), raw_size, disk_data.get(), cache_size,
          ddict);
    } else {
      decompressed_size = ZSTD_decompressDCtx(
          zstd_dctx_, raw_data.get(), raw_size, disk_data.get(), cache_size);
    }
    if (ZSTD_isError(decompressed_size)) {
      Debug("decompression failed: %s\n", ZSTD_getErrorName(decompressed_size));
      return;
    }
    if (decompressed_size != raw_size) {
      Debug("decompressed size mismatch: expected %d, actual %d\n",
            raw_size,
            decompressed_size);
      return;
    }
    entry->cache.reset(new ScriptCompiler::CachedData(
        raw_data.release(), raw_size, ScriptCompiler::CachedData::BufferOwned));
  }
  Debug(" success, size=%d\n", raw_size);
}

static std::string GetRelativePath(std::string_view path,
                                   std::string_view base) {
// On Windows, the native encoding is UTF-16, so we need to convert
// the paths to wide strings before using std::filesystem::path.
// On other platforms, std::filesystem::path can handle UTF-8 directly.
#ifdef _WIN32
  std::filesystem::path module_path(ConvertUTF8ToWideString(std::string(path)));
  std::filesystem::path base_path(ConvertUTF8ToWideString(std::string(base)));
#else
  std::filesystem::path module_path(path);
  std::filesystem::path base_path(base);
#endif
  std::filesystem::path relative = module_path.lexically_relative(base_path);
  auto u8str = relative.u8string();
  return std::string(u8str.begin(), u8str.end());
}

CompileCacheEntry* CompileCacheHandler::GetOrInsert(Local<String> code,
                                                    Local<String> filename,
                                                    CachedCodeType type) {
  DCHECK(!compile_cache_dir_.empty());

  Environment* env = Environment::GetCurrent(isolate_->GetCurrentContext());
  Utf8Value filename_utf8(isolate_, filename);
  std::string file_path = filename_utf8.ToString();
  // If the portable cache is enabled and it seems possible to compute the
  // relative position from an absolute path, we use the relative position
  // in the cache key.
  if (portable_ == EnableOption::PORTABLE && IsAbsoluteFilePath(file_path)) {
    // Normalize the path to ensure it is consistent.
    std::string normalized_file_path = NormalizeFileURLOrPath(env, file_path);
    if (normalized_file_path.empty()) {
      return nullptr;
    }
    std::string relative_path =
        GetRelativePath(normalized_file_path, normalized_compile_cache_dir_);
    if (!relative_path.empty()) {
      file_path = relative_path;
      Debug("[compile cache] using relative path %s from %s\n",
            file_path,
            compile_cache_dir_);
    }
  }
  uint32_t key = GetCacheKey(file_path, type);

  // TODO(joyeecheung): don't encode this again into UTF8. If we read the
  // UTF8 content on disk as raw buffer (from the JS layer, while watching out
  // for monkey patching), we can just hash it directly.
  Utf8Value code_utf8(isolate_, code);
  uint32_t code_hash = GetHash(code_utf8.out(), code_utf8.length());
  auto loaded = compiler_cache_store_.find(key);

  // TODO(joyeecheung): let V8's in-isolate compilation cache take precedence.
  if (loaded != compiler_cache_store_.end() &&
      loaded->second->code_hash == code_hash) {
    return loaded->second.get();
  }

  // If the code hash mismatches, the code has changed, reset the stale
  // entry in place. Otherwise insert a new one.
  CompileCacheEntry* result;
  if (loaded != compiler_cache_store_.end()) {
    result = loaded->second.get();
    result->refreshed = false;
    result->persisted = false;
  } else {
    result = compiler_cache_store_
                 .emplace(key, std::make_unique<CompileCacheEntry>())
                 .first->second.get();
  }

  result->code_hash = code_hash;
  result->code_size = code_utf8.length();
  result->cache_key = key;
  result->cache_filename =
      compile_cache_dir_ + kPathSeparator + Uint32ToHex(key);
  result->source_filename = filename_utf8.ToString();
  result->cache = nullptr;
  result->type = type;

  // TODO(joyeecheung): if we fail enough times, stop trying for any future
  // files.
  ReadCacheFile(result);

  return result;
}

ScriptCompiler::CachedData* SerializeCodeCache(Local<Function> func) {
  return ScriptCompiler::CreateCodeCacheForFunction(func);
}

ScriptCompiler::CachedData* SerializeCodeCache(Local<Module> mod) {
  return ScriptCompiler::CreateCodeCache(mod->GetUnboundModuleScript());
}

template <typename T>
void CompileCacheHandler::MaybeSaveImpl(CompileCacheEntry* entry,
                                        Local<T> func_or_mod,
                                        bool rejected) {
  DCHECK_NOT_NULL(entry);
  Debug("[compile cache] V8 code cache for %s %s was %s, ",
        entry->type_name(),
        entry->source_filename,
        rejected                    ? "rejected"
        : (entry->cache == nullptr) ? "not initialized"
                                    : "accepted");
  if (entry->cache != nullptr && !rejected) {  // accepted
    Debug("keeping the in-memory entry\n");
    return;
  }
  Debug("%s the in-memory entry\n",
        entry->cache == nullptr ? "initializing" : "refreshing");

  ScriptCompiler::CachedData* data = SerializeCodeCache(func_or_mod);
  DCHECK_EQ(data->buffer_policy, ScriptCompiler::CachedData::BufferOwned);
  entry->refreshed = true;
  entry->cache.reset(data);
}

void CompileCacheHandler::MaybeSave(CompileCacheEntry* entry,
                                    Local<Module> mod,
                                    bool rejected) {
  DCHECK(mod->IsSourceTextModule());
  MaybeSaveImpl(entry, mod, rejected);
}

void CompileCacheHandler::MaybeSave(CompileCacheEntry* entry,
                                    Local<Function> func,
                                    bool rejected) {
  MaybeSaveImpl(entry, func, rejected);
}

void CompileCacheHandler::MaybeSave(CompileCacheEntry* entry,
                                    std::string_view transpiled) {
  CHECK(entry->type == CachedCodeType::kStrippedTypeScript);
  Debug("[compile cache] saving transpilation cache for %s %s\n",
        entry->type_name(),
        entry->source_filename);

  // TODO(joyeecheung): it's weird to copy it again here. Convert the v8::String
  // directly into buffer held by v8::ScriptCompiler::CachedData here.
  int cache_size = static_cast<int>(transpiled.size());
  uint8_t* data = new uint8_t[cache_size];
  memcpy(data, transpiled.data(), cache_size);
  entry->cache.reset(new ScriptCompiler::CachedData(
      data, cache_size, ScriptCompiler::CachedData::BufferOwned));
  entry->refreshed = true;
}

/**
 * Persist the compile cache accumulated in memory to disk.
 *
 * To avoid race conditions, the cache file includes hashes of the original
 * source code and the cache content. It's first written to a temporary file
 * before being renamed to the target name.
 *
 * Layout of a cache file:
 *   [uint32_t] magic number
 *   [uint32_t] code size
 *   [uint32_t] code hash
 *   [uint32_t] cache size
 *   [uint32_t] cache hash
 *   .... compile cache content ....
 */
void CompileCacheHandler::Persist() {
  DCHECK(!compile_cache_dir_.empty());

  // TODO(joyeecheung): do this using a separate event loop to utilize the
  // libuv thread pool and do the file system operations concurrently.
  // TODO(joyeecheung): Currently flushing is triggered by either process
  // shutdown or user requests. In the future we should simply start the
  // writes right after module loading on a separate thread, and this method
  // only blocks until all the pending writes (if any) on the other thread are
  // finished. In that case, the off-thread writes should finish long
  // before any attempt of flushing is made so the method would then only
  // incur a negligible overhead from thread synchronization.

  // The compression context is created lazily when there is anything to
  // compress and reused for all the entries in this invocation.
  ZSTD_CCtx* cctx = nullptr;
  auto cleanup_cctx = OnScopeLeave([&cctx]() {
    if (cctx != nullptr) {
      ZSTD_freeCCtx(cctx);
    }
  });

  for (auto& pair : compiler_cache_store_) {
    auto* entry = pair.second.get();
    const char* type_name = entry->type_name();
    if (entry->cache == nullptr) {
      Debug("[compile cache] skip persisting %s %s because the cache was not "
            "initialized\n",
            type_name,
            entry->source_filename);
      continue;
    }
    if (entry->refreshed == false) {
      Debug(
          "[compile cache] skip persisting %s %s because cache was the same\n",
          type_name,
          entry->source_filename);
      continue;
    }
    if (entry->persisted == true) {
      Debug("[compile cache] skip persisting %s %s because cache was already "
            "persisted\n",
            type_name,
            entry->source_filename);
      continue;
    }

    DCHECK_EQ(entry->cache->buffer_policy,
              ScriptCompiler::CachedData::BufferOwned);
    char* raw_ptr =
        reinterpret_cast<char*>(const_cast<uint8_t*>(entry->cache->data));
    uint32_t raw_size = static_cast<uint32_t>(entry->cache->length);

    // Compress the cache with zstd to reduce the size on disk. Compression
    // level 1 prioritizes speed - persistence usually happens on process
    // shutdown and should add as little overhead as possible. If the data
    // is not compressible, store it uncompressed, which is indicated by
    // the cache size being equal to the uncompressed size in the headers.
    //
    // We also try the embedded trained dictionary and keep whichever frame is
    // smaller (still subject to the "only store if < raw" policy). The
    // dictionary mainly helps the small/medium caches that dominate real
    // compile cache usage; for inputs where plain zstd already wins we keep
    // the plain frame.
    char* cache_ptr = raw_ptr;
    uint32_t cache_size = raw_size;
    std::unique_ptr<uint8_t[]> compressed;
    std::unique_ptr<uint8_t[]> compressed_dict;
    if (cctx != nullptr || (cctx = ZSTD_createCCtx()) != nullptr) {
      size_t compressed_bound = ZSTD_compressBound(raw_size);
      compressed.reset(new uint8_t[compressed_bound]);
      size_t compressed_size = ZSTD_compressCCtx(
          cctx, compressed.get(), compressed_bound, raw_ptr, raw_size, 1);
      char* best_ptr = reinterpret_cast<char*>(compressed.get());
      // Only attempt the dictionary for small/medium entries (see
      // kCompileCacheDictMaxRawSize); for large blobs it never wins and the
      // extra compression would be wasted work.
      ZSTD_CDict* cdict = raw_size <= kCompileCacheDictMaxRawSize
                              ? GetCompileCacheCDict()
                              : nullptr;
      if (cdict != nullptr) {
        // Compress into a separate buffer so the selected frame's bytes and
        // size always stay in sync (the plain buffer is left untouched).
        compressed_dict.reset(new uint8_t[compressed_bound]);
        size_t dict_size = ZSTD_compress_usingCDict(
            cctx, compressed_dict.get(), compressed_bound, raw_ptr, raw_size,
            cdict);
        if (!ZSTD_isError(dict_size) &&
            (ZSTD_isError(compressed_size) || dict_size < compressed_size)) {
          compressed_size = dict_size;
          best_ptr = reinterpret_cast<char*>(compressed_dict.get());
        }
      }
      if (!ZSTD_isError(compressed_size) && compressed_size < raw_size) {
        cache_ptr = best_ptr;
        cache_size = static_cast<uint32_t>(compressed_size);
      }
    }
    Debug("[compile cache] compressed cache for %s %s: %d -> %d bytes\n",
          type_name,
          entry->source_filename,
          raw_size,
          cache_size);

    uint32_t cache_hash = GetHash(cache_ptr, cache_size);

    // Generating headers.
    std::array<uint32_t, kHeaderCount> headers;
    headers[kMagicNumberOffset] = kCacheMagicNumber;
    headers[kCodeSizeOffset] = entry->code_size;
    headers[kCacheSizeOffset] = cache_size;
    headers[kCodeHashOffset] = entry->code_hash;
    headers[kCacheHashOffset] = cache_hash;
    headers[kCacheRawSizeOffset] = raw_size;

    // Generate the temporary filename.
    // The temporary file should be placed in a location like:
    //
    // $NODE_COMPILE_CACHE_DIR/v23.0.0-pre-arm64-5fad6d45-501/e7f8ef7f.cache.tcqrsK
    //
    // 1. $NODE_COMPILE_CACHE_DIR either comes from the $NODE_COMPILE_CACHE
    // environment
    //    variable or `module.enableCompileCache()`.
    // 2. v23.0.0-pre-arm64-5fad6d45-501 is the sub cache directory and
    //    e7f8ef7f is the hash for the cache (see
    //    CompileCacheHandler::Enable()),
    // 3. tcqrsK is generated by uv_fs_mkstemp() as a temporary identifier.
    uv_fs_t mkstemp_req;
    auto cleanup_mkstemp =
        OnScopeLeave([&mkstemp_req]() { uv_fs_req_cleanup(&mkstemp_req); });
    std::string cache_filename_tmp = entry->cache_filename + ".XXXXXX";
    Debug("[compile cache] Creating temporary file for cache of %s (%s)...",
          entry->source_filename,
          type_name);
    int err = uv_fs_mkstemp(
        nullptr, &mkstemp_req, cache_filename_tmp.c_str(), nullptr);
    if (err < 0) {
      Debug("failed. %s\n", uv_strerror(err));
      continue;
    }
    Debug(" -> %s\n", mkstemp_req.path);
    Debug("[compile cache] writing cache for %s %s to temporary file %s [%d "
          "%d %d "
          "%d %d %d]...",
          type_name,
          entry->source_filename,
          mkstemp_req.path,
          headers[kMagicNumberOffset],
          headers[kCodeSizeOffset],
          headers[kCacheSizeOffset],
          headers[kCodeHashOffset],
          headers[kCacheHashOffset],
          headers[kCacheRawSizeOffset]);

    // Write to the temporary file.
    uv_buf_t headers_buf =
        uv_buf_init(reinterpret_cast<char*>(headers.data()), kHeaderSize);
    uv_buf_t data_buf = uv_buf_init(cache_ptr, cache_size);
    uv_buf_t bufs[] = {headers_buf, data_buf};

    uv_fs_t write_req;
    auto cleanup_write =
        OnScopeLeave([&write_req]() { uv_fs_req_cleanup(&write_req); });
    err = uv_fs_write(
        nullptr, &write_req, mkstemp_req.result, bufs, 2, 0, nullptr);
    if (err < 0) {
      Debug("failed: %s\n", uv_strerror(err));
      continue;
    }

    uv_fs_t close_req;
    auto cleanup_close =
        OnScopeLeave([&close_req]() { uv_fs_req_cleanup(&close_req); });
    err = uv_fs_close(nullptr, &close_req, mkstemp_req.result, nullptr);

    if (err < 0) {
      Debug("failed: %s\n", uv_strerror(err));
      continue;
    }

    Debug("success\n");

    // Rename the temporary file to the actual cache file.
    uv_fs_t rename_req;
    auto cleanup_rename =
        OnScopeLeave([&rename_req]() { uv_fs_req_cleanup(&rename_req); });
    std::string cache_filename_final = entry->cache_filename;
    Debug("[compile cache] Renaming %s to %s...",
          mkstemp_req.path,
          cache_filename_final);
    err = uv_fs_rename(nullptr,
                       &rename_req,
                       mkstemp_req.path,
                       cache_filename_final.c_str(),
                       nullptr);
    if (err < 0) {
      Debug("failed: %s\n", uv_strerror(err));
      continue;
    }
    Debug("success\n");
    entry->persisted = true;
  }

  // Clear the map at the end in one go instead of during the iteration to
  // avoid rehashing costs.
  Debug("[compile cache] Clear deserialized cache.\n");
  compiler_cache_store_.clear();
}

CompileCacheHandler::CompileCacheHandler(Environment* env)
    : isolate_(env->isolate()),
      is_debug_(
          env->enabled_debug_list()->enabled(DebugCategory::COMPILE_CACHE)) {}

CompileCacheHandler::~CompileCacheHandler() {
  if (zstd_dctx_ != nullptr) {
    ZSTD_freeDCtx(zstd_dctx_);
  }
}

// Directory structure:
// - Compile cache directory (from NODE_COMPILE_CACHE)
//   - $NODE_VERSION-$ARCH-$CACHE_DATA_VERSION_TAG-$UID
//     - $FILENAME_AND_MODULE_TYPE_HASH.cache: a hash of filename + module type
CompileCacheEnableResult CompileCacheHandler::Enable(Environment* env,
                                                     const std::string& dir,
                                                     EnableOption option) {
  std::string cache_tag = GetCacheVersionTag();
  std::string absolute_cache_dir_base = PathResolve(env, {dir});
  std::string cache_dir_with_tag =
      absolute_cache_dir_base + kPathSeparator + cache_tag;
  CompileCacheEnableResult result;
  Debug("[compile cache] resolved path %s + %s -> %s\n",
        dir,
        cache_tag,
        cache_dir_with_tag);

  if (!env->permission()->is_granted(
          env,
          permission::PermissionScope::kFileSystemWrite,
          cache_dir_with_tag)) [[unlikely]] {
    result.message = "Skipping compile cache because write permission for " +
                     cache_dir_with_tag + " is not granted";
    result.status = CompileCacheEnableStatus::FAILED;
    return result;
  }

  if (!env->permission()->is_granted(
          env,
          permission::PermissionScope::kFileSystemRead,
          cache_dir_with_tag)) [[unlikely]] {
    result.message = "Skipping compile cache because read permission for " +
                     cache_dir_with_tag + " is not granted";
    result.status = CompileCacheEnableStatus::FAILED;
    return result;
  }

  fs::FSReqWrapSync req_wrap;
  int err = fs::MKDirpSync(
      nullptr, &(req_wrap.req), cache_dir_with_tag, 0777, nullptr);
  if (is_debug_) {
    Debug("[compile cache] creating cache directory %s...%s\n",
          cache_dir_with_tag,
          err < 0 ? uv_strerror(err) : "success");
  }
  if (err != 0 && err != UV_EEXIST) {
    result.message =
        "Cannot create cache directory: " + std::string(uv_strerror(err));
    result.status = CompileCacheEnableStatus::FAILED;
    return result;
  }

  result.cache_directory = absolute_cache_dir_base;
  compile_cache_dir_ = cache_dir_with_tag;
  portable_ = option;
  if (option == EnableOption::PORTABLE) {
    normalized_compile_cache_dir_ =
        NormalizeFileURLOrPath(env, compile_cache_dir_);
  }
  result.status = CompileCacheEnableStatus::ENABLED;
  return result;
}

}  // namespace node
