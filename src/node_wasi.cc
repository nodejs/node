#include "env-inl.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "memory_tracker-inl.h"
#include "node_mem-inl.h"
#include "util-inl.h"
#include "node.h"
#include "uv.h"
#include "uvwasi.h"
#include "node_wasi.h"

namespace node {
namespace wasi {

static inline bool is_access_oob(size_t mem_size,
                                 uint32_t offset,
                                 uint32_t buf_size) {
  return offset + buf_size > mem_size;
}

template <typename... Args>
inline void Debug(WASI* wasi, Args&&... args) {
  Debug(wasi->env(), DebugCategory::WASI, std::forward<Args>(args)...);
}

#define RETURN_IF_BAD_ARG_COUNT(args, expected)                               \
  do {                                                                        \
    if ((args).Length() != (expected)) {                                      \
      (args).GetReturnValue().Set(UVWASI_EINVAL);                             \
      return;                                                                 \
    }                                                                         \
  } while (0)

#define CHECK_TO_TYPE_OR_RETURN(args, input, type, result)                    \
  do {                                                                        \
    if (!(input)->Is##type()) {                                               \
      (args).GetReturnValue().Set(UVWASI_EINVAL);                             \
      return;                                                                 \
    }                                                                         \
    (result) = (input).As<type>()->Value();                                   \
  } while (0)

#define UNWRAP_BIGINT_OR_RETURN(args, input, type, result)                    \
  do {                                                                        \
    if (!(input)->IsBigInt()) {                                               \
      (args).GetReturnValue().Set(UVWASI_EINVAL);                             \
      return;                                                                 \
    }                                                                         \
    Local<BigInt> js_value = (input).As<BigInt>();                            \
    bool lossless;                                                            \
    (result) = js_value->type ## Value(&lossless);                            \
  } while (0)

#define GET_BACKING_STORE_OR_RETURN(wasi, args, mem_ptr, mem_size)            \
  do {                                                                        \
    uvwasi_errno_t err = (wasi)->backingStore((mem_ptr), (mem_size));         \
    if (err != UVWASI_ESUCCESS) {                                             \
      (args).GetReturnValue().Set(err);                                       \
      return;                                                                 \
    }                                                                         \
  } while (0)

#define CHECK_BOUNDS_OR_RETURN(args, mem_size, offset, buf_size)              \
  do {                                                                        \
    if (is_access_oob((mem_size), (offset), (buf_size))) {                    \
      (args).GetReturnValue().Set(UVWASI_EOVERFLOW);                          \
      return;                                                                 \
    }                                                                         \
  } while (0)


using v8::Array;
using v8::ArrayBuffer;
using v8::BackingStore;
using v8::BigInt;
using v8::Context;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;


static MaybeLocal<Value> WASIException(Local<Context> context,
                                       int errorno,
                                       const char* syscall) {
  Isolate* isolate = context->GetIsolate();
  Environment* env = Environment::GetCurrent(context);
  CHECK_NOT_NULL(env);
  const char* err_name = uvwasi_embedder_err_code_to_string(errorno);
  Local<String> js_code = OneByteString(isolate, err_name);
  Local<String> js_syscall = OneByteString(isolate, syscall);
  Local<String> js_msg = js_code;
  js_msg =
      String::Concat(isolate, js_msg, FIXED_ONE_BYTE_STRING(isolate, ", "));
  js_msg = String::Concat(isolate, js_msg, js_syscall);
  Local<Object> e =
    Exception::Error(js_msg)->ToObject(context)
      .ToLocalChecked();

  if (e->Set(context,
             env->errno_string(),
             Integer::New(isolate, errorno)).IsNothing() ||
      e->Set(context, env->code_string(), js_code).IsNothing() ||
      e->Set(context, env->syscall_string(), js_syscall).IsNothing()) {
    return MaybeLocal<Value>();
  }

  return e;
}


WASI::WASI(Environment* env,
           Local<Object> object,
           uvwasi_options_t* options) : BaseObject(env, object) {
  MakeWeak();
  alloc_info_ = MakeAllocator();
  options->allocator = &alloc_info_;
  int err = uvwasi_init(&uvw_, options);
  if (err != UVWASI_ESUCCESS) {
    Local<Context> context = env->context();
    MaybeLocal<Value> exception = WASIException(context, err, "uvwasi_init");

    if (exception.IsEmpty())
      return;

    context->GetIsolate()->ThrowException(exception.ToLocalChecked());
  }
}


WASI::~WASI() {
  uvwasi_destroy(&uvw_);
  CHECK_EQ(current_uvwasi_memory_, 0);
}

void WASI::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("memory", memory_);
  tracker->TrackFieldWithSize("uvwasi_memory", current_uvwasi_memory_);
}

void WASI::CheckAllocatedSize(size_t previous_size) const {
  CHECK_GE(current_uvwasi_memory_, previous_size);
}

void WASI::IncreaseAllocatedSize(size_t size) {
  current_uvwasi_memory_ += size;
}

void WASI::DecreaseAllocatedSize(size_t size) {
  current_uvwasi_memory_ -= size;
}

void WASI::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK_EQ(args.Length(), 3);
  CHECK(args[0]->IsArray());
  CHECK(args[1]->IsArray());
  CHECK(args[2]->IsArray());

  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  Local<Array> argv = args[0].As<Array>();
  const uint32_t argc = argv->Length();
  uvwasi_options_t options;

  options.in = 0;
  options.out = 1;
  options.err = 2;
  options.fd_table_size = 3;
  options.argc = argc;
  options.argv = argc == 0 ? nullptr : new char*[argc];

  for (uint32_t i = 0; i < argc; i++) {
    auto arg = argv->Get(context, i).ToLocalChecked();
    CHECK(arg->IsString());
    node::Utf8Value str(env->isolate(), arg);
    options.argv[i] = strdup(*str);
    CHECK_NOT_NULL(options.argv[i]);
  }

  Local<Array> env_pairs = args[1].As<Array>();
  const uint32_t envc = env_pairs->Length();
  options.envp = new char*[envc + 1];
  for (uint32_t i = 0; i < envc; i++) {
    auto pair = env_pairs->Get(context, i).ToLocalChecked();
    CHECK(pair->IsString());
    node::Utf8Value str(env->isolate(), pair);
    options.envp[i] = strdup(*str);
    CHECK_NOT_NULL(options.envp[i]);
  }
  options.envp[envc] = nullptr;

  Local<Array> preopens = args[2].As<Array>();
  CHECK_EQ(preopens->Length() % 2, 0);
  options.preopenc = preopens->Length() / 2;
  options.preopens = Calloc<uvwasi_preopen_t>(options.preopenc);
  int index = 0;
  for (uint32_t i = 0; i < preopens->Length(); i += 2) {
    auto mapped = preopens->Get(context, i).ToLocalChecked();
    auto real = preopens->Get(context, i + 1).ToLocalChecked();
    CHECK(mapped->IsString());
    CHECK(real->IsString());
    node::Utf8Value mapped_path(env->isolate(), mapped);
    node::Utf8Value real_path(env->isolate(), real);
    options.preopens[index].mapped_path = strdup(*mapped_path);
    CHECK_NOT_NULL(options.preopens[index].mapped_path);
    options.preopens[index].real_path = strdup(*real_path);
    CHECK_NOT_NULL(options.preopens[index].real_path);
    index++;
  }

  new WASI(env, args.This(), &options);

  if (options.argv != nullptr) {
    for (uint32_t i = 0; i < argc; i++)
      free(options.argv[i]);
    delete[] options.argv;
  }

  if (options.envp != nullptr) {
    for (uint32_t i = 0; options.envp[i]; i++)
      free(options.envp[i]);
    delete[] options.envp;
  }

  if (options.preopens != nullptr) {
    for (uint32_t i = 0; i < options.preopenc; i++) {
      free(options.preopens[i].mapped_path);
      free(options.preopens[i].real_path);
    }

    delete[] options.preopens;
  }
}


void WASI::ArgsGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t argv_offset;
  uint32_t argv_buf_offset;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, argv_offset);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, argv_buf_offset);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "args_get(%d, %d)\n", argv_offset, argv_buf_offset);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args,
                         mem_size,
                         argv_buf_offset,
                         wasi->uvw_.argv_buf_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, argv_offset, wasi->uvw_.argc * 4);
  std::vector<char*> argv(wasi->uvw_.argc);
  char* argv_buf = &memory[argv_buf_offset];
  uvwasi_errno_t err = uvwasi_args_get(&wasi->uvw_, argv.data(), argv_buf);

  if (err == UVWASI_ESUCCESS) {
    for (size_t i = 0; i < wasi->uvw_.argc; i++) {
      uint32_t offset = argv_buf_offset + (argv[i] - argv[0]);
      wasi->writeUInt32(memory, offset, argv_offset + (i * 4));
    }
  }

  args.GetReturnValue().Set(err);
}


void WASI::ArgsSizesGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t argc_offset;
  uint32_t argv_buf_offset;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, argc_offset);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, argv_buf_offset);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "args_sizes_get(%d, %d)\n", argc_offset, argv_buf_offset);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, argc_offset, 4);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, argv_buf_offset, 4);
  size_t argc;
  size_t argv_buf_size;
  uvwasi_errno_t err = uvwasi_args_sizes_get(&wasi->uvw_,
                                             &argc,
                                             &argv_buf_size);
  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt32(memory, argc, argc_offset);
    wasi->writeUInt32(memory, argv_buf_size, argv_buf_offset);
  }

  args.GetReturnValue().Set(err);
}


void WASI::ClockResGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t clock_id;
  uint32_t resolution_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, clock_id);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, resolution_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "clock_res_get(%d, %d)\n", clock_id, resolution_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, resolution_ptr, 8);
  uvwasi_timestamp_t resolution;
  uvwasi_errno_t err = uvwasi_clock_res_get(&wasi->uvw_,
                                            clock_id,
                                            &resolution);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt64(memory, resolution, resolution_ptr);

  args.GetReturnValue().Set(err);
}


void WASI::ClockTimeGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t clock_id;
  uint64_t precision;
  uint32_t time_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 3);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, clock_id);
  UNWRAP_BIGINT_OR_RETURN(args, args[1], Uint64, precision);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, time_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "clock_time_get(%d, %d, %d)\n", clock_id, precision, time_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, time_ptr, 8);
  uvwasi_timestamp_t time;
  uvwasi_errno_t err = uvwasi_clock_time_get(&wasi->uvw_,
                                             clock_id,
                                             precision,
                                             &time);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt64(memory, time, time_ptr);

  args.GetReturnValue().Set(err);
}


void WASI::EnvironGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t environ_offset;
  uint32_t environ_buf_offset;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, environ_offset);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, environ_buf_offset);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "environ_get(%d, %d)\n", environ_offset, environ_buf_offset);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args,
                         mem_size,
                         environ_buf_offset,
                         wasi->uvw_.env_buf_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, environ_offset, wasi->uvw_.envc * 4);
  std::vector<char*> environment(wasi->uvw_.envc);
  char* environ_buf = &memory[environ_buf_offset];
  uvwasi_errno_t err = uvwasi_environ_get(&wasi->uvw_,
                                          environment.data(),
                                          environ_buf);

  if (err == UVWASI_ESUCCESS) {
    for (size_t i = 0; i < wasi->uvw_.envc; i++) {
      uint32_t offset = environ_buf_offset + (environment[i] - environment[0]);
      wasi->writeUInt32(memory, offset, environ_offset + (i * 4));
    }
  }

  args.GetReturnValue().Set(err);
}


void WASI::EnvironSizesGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t envc_offset;
  uint32_t env_buf_offset;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, envc_offset);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, env_buf_offset);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "environ_sizes_get(%d, %d)\n", envc_offset, env_buf_offset);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, envc_offset, 4);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, env_buf_offset, 4);
  size_t envc;
  size_t env_buf_size;
  uvwasi_errno_t err = uvwasi_environ_sizes_get(&wasi->uvw_,
                                                &envc,
                                                &env_buf_size);
  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt32(memory, envc, envc_offset);
    wasi->writeUInt32(memory, env_buf_size, env_buf_offset);
  }

  args.GetReturnValue().Set(err);
}


void WASI::FdAdvise(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint64_t offset;
  uint64_t len;
  uint8_t advice;
  RETURN_IF_BAD_ARG_COUNT(args, 4);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  UNWRAP_BIGINT_OR_RETURN(args, args[1], Uint64, offset);
  UNWRAP_BIGINT_OR_RETURN(args, args[2], Uint64, len);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, advice);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_advise(%d, %d, %d, %d)\n", fd, offset, len, advice);
  uvwasi_errno_t err = uvwasi_fd_advise(&wasi->uvw_, fd, offset, len, advice);
  args.GetReturnValue().Set(err);
}


void WASI::FdAllocate(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint64_t offset;
  uint64_t len;
  RETURN_IF_BAD_ARG_COUNT(args, 3);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  UNWRAP_BIGINT_OR_RETURN(args, args[1], Uint64, offset);
  UNWRAP_BIGINT_OR_RETURN(args, args[2], Uint64, len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_allocate(%d, %d, %d)\n", fd, offset, len);
  uvwasi_errno_t err = uvwasi_fd_allocate(&wasi->uvw_, fd, offset, len);
  args.GetReturnValue().Set(err);
}


void WASI::FdClose(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  RETURN_IF_BAD_ARG_COUNT(args, 1);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_close(%d)\n", fd);
  uvwasi_errno_t err = uvwasi_fd_close(&wasi->uvw_, fd);
  args.GetReturnValue().Set(err);
}


void WASI::FdDatasync(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  RETURN_IF_BAD_ARG_COUNT(args, 1);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_datasync(%d)\n", fd);
  uvwasi_errno_t err = uvwasi_fd_datasync(&wasi->uvw_, fd);
  args.GetReturnValue().Set(err);
}


void WASI::FdFdstatGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t buf;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, buf);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_fdstat_get(%d, %d)\n", fd, buf);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, buf, 24);
  uvwasi_fdstat_t stats;
  uvwasi_errno_t err = uvwasi_fd_fdstat_get(&wasi->uvw_, fd, &stats);

  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt8(memory, stats.fs_filetype, buf);
    wasi->writeUInt16(memory, stats.fs_flags, buf + 2);
    wasi->writeUInt64(memory, stats.fs_rights_base, buf + 8);
    wasi->writeUInt64(memory, stats.fs_rights_inheriting, buf + 16);
  }

  args.GetReturnValue().Set(err);
}


void WASI::FdFdstatSetFlags(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint16_t flags;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, flags);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_fdstat_set_flags(%d, %d)\n", fd, flags);
  uvwasi_errno_t err = uvwasi_fd_fdstat_set_flags(&wasi->uvw_, fd, flags);
  args.GetReturnValue().Set(err);
}


void WASI::FdFdstatSetRights(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint64_t fs_rights_base;
  uint64_t fs_rights_inheriting;
  RETURN_IF_BAD_ARG_COUNT(args, 3);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  UNWRAP_BIGINT_OR_RETURN(args, args[1], Uint64, fs_rights_base);
  UNWRAP_BIGINT_OR_RETURN(args, args[2], Uint64, fs_rights_inheriting);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "fd_fdstat_set_rights(%d, %d, %d)\n",
        fd,
        fs_rights_base,
        fs_rights_inheriting);
  uvwasi_errno_t err = uvwasi_fd_fdstat_set_rights(&wasi->uvw_,
                                                   fd,
                                                   fs_rights_base,
                                                   fs_rights_inheriting);
  args.GetReturnValue().Set(err);
}


void WASI::FdFilestatGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t buf;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, buf);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_filestat_get(%d, %d)\n", fd, buf);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, buf, 64);
  uvwasi_filestat_t stats;
  uvwasi_errno_t err = uvwasi_fd_filestat_get(&wasi->uvw_, fd, &stats);

  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt64(memory, stats.st_dev, buf);
    wasi->writeUInt64(memory, stats.st_ino, buf + 8);
    wasi->writeUInt8(memory, stats.st_filetype, buf + 16);
    wasi->writeUInt64(memory, stats.st_nlink, buf + 24);
    wasi->writeUInt64(memory, stats.st_size, buf + 32);
    wasi->writeUInt64(memory, stats.st_atim, buf + 40);
    wasi->writeUInt64(memory, stats.st_mtim, buf + 48);
    wasi->writeUInt64(memory, stats.st_ctim, buf + 56);
  }

  args.GetReturnValue().Set(err);
}


void WASI::FdFilestatSetSize(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint64_t st_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  UNWRAP_BIGINT_OR_RETURN(args, args[1], Uint64, st_size);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_filestat_set_size(%d, %d)\n", fd, st_size);
  uvwasi_errno_t err = uvwasi_fd_filestat_set_size(&wasi->uvw_, fd, st_size);
  args.GetReturnValue().Set(err);
}


void WASI::FdFilestatSetTimes(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint64_t st_atim;
  uint64_t st_mtim;
  uint16_t fst_flags;
  RETURN_IF_BAD_ARG_COUNT(args, 4);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  UNWRAP_BIGINT_OR_RETURN(args, args[1], Uint64, st_atim);
  UNWRAP_BIGINT_OR_RETURN(args, args[2], Uint64, st_mtim);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, fst_flags);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "fd_filestat_set_times(%d, %d, %d, %d)\n",
        fd,
        st_atim,
        st_mtim,
        fst_flags);
  uvwasi_errno_t err = uvwasi_fd_filestat_set_times(&wasi->uvw_,
                                                    fd,
                                                    st_atim,
                                                    st_mtim,
                                                    fst_flags);
  args.GetReturnValue().Set(err);
}


void WASI::FdPread(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t iovs_ptr;
  uint32_t iovs_len;
  uint64_t offset;
  uint32_t nread_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 5);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, iovs_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, iovs_len);
  UNWRAP_BIGINT_OR_RETURN(args, args[3], Uint64, offset);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, nread_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "uvwasi_fd_pread(%d, %d, %d, %d, %d)\n",
        fd,
        iovs_ptr,
        iovs_len,
        offset,
        nread_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, iovs_ptr, iovs_len * 8);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, nread_ptr, 4);
  uvwasi_iovec_t* iovs = UncheckedCalloc<uvwasi_iovec_t>(iovs_len);

  if (iovs == nullptr) {
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  for (uint32_t i = 0; i < iovs_len; ++i) {
    uint32_t buf_ptr;
    uint32_t buf_len;

    wasi->readUInt32(memory, &buf_ptr, iovs_ptr);
    wasi->readUInt32(memory, &buf_len, iovs_ptr + 4);

    if (is_access_oob(mem_size, buf_ptr, buf_len)) {
      free(iovs);
      args.GetReturnValue().Set(UVWASI_EOVERFLOW);
      return;
    }

    iovs_ptr += 8;
    iovs[i].buf = static_cast<void*>(&memory[buf_ptr]);
    iovs[i].buf_len = buf_len;
  }

  size_t nread;
  uvwasi_errno_t err = uvwasi_fd_pread(&wasi->uvw_,
                                       fd,
                                       iovs,
                                       iovs_len,
                                       offset,
                                       &nread);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, nread, nread_ptr);

  free(iovs);
  args.GetReturnValue().Set(err);
}


void WASI::FdPrestatGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t buf;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, buf);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_prestat_get(%d, %d)\n", fd, buf);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, buf, 8);
  uvwasi_prestat_t prestat;
  uvwasi_errno_t err = uvwasi_fd_prestat_get(&wasi->uvw_, fd, &prestat);

  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt32(memory, prestat.pr_type, buf);
    wasi->writeUInt32(memory, prestat.u.dir.pr_name_len, buf + 4);
  }

  args.GetReturnValue().Set(err);
}


void WASI::FdPrestatDirName(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t path_ptr;
  uint32_t path_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 3);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_prestat_dir_name(%d, %d, %d)\n", fd, path_ptr, path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  uvwasi_errno_t err = uvwasi_fd_prestat_dir_name(&wasi->uvw_,
                                                  fd,
                                                  &memory[path_ptr],
                                                  path_len);
  args.GetReturnValue().Set(err);
}


void WASI::FdPwrite(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t iovs_ptr;
  uint32_t iovs_len;
  uint64_t offset;
  uint32_t nwritten_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 5);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, iovs_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, iovs_len);
  UNWRAP_BIGINT_OR_RETURN(args, args[3], Uint64, offset);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, nwritten_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "uvwasi_fd_pwrite(%d, %d, %d, %d, %d)\n",
        fd,
        iovs_ptr,
        iovs_len,
        offset,
        nwritten_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, iovs_ptr, iovs_len * 8);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, nwritten_ptr, 4);
  uvwasi_ciovec_t* iovs = UncheckedCalloc<uvwasi_ciovec_t>(iovs_len);

  if (iovs == nullptr) {
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  for (uint32_t i = 0; i < iovs_len; ++i) {
    uint32_t buf_ptr;
    uint32_t buf_len;

    wasi->readUInt32(memory, &buf_ptr, iovs_ptr);
    wasi->readUInt32(memory, &buf_len, iovs_ptr + 4);

    if (is_access_oob(mem_size, buf_ptr, buf_len)) {
      free(iovs);
      args.GetReturnValue().Set(UVWASI_EOVERFLOW);
      return;
    }

    iovs_ptr += 8;
    iovs[i].buf = static_cast<void*>(&memory[buf_ptr]);
    iovs[i].buf_len = buf_len;
  }

  size_t nwritten;
  uvwasi_errno_t err = uvwasi_fd_pwrite(&wasi->uvw_,
                                        fd,
                                        iovs,
                                        iovs_len,
                                        offset,
                                        &nwritten);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, nwritten, nwritten_ptr);

  free(iovs);
  args.GetReturnValue().Set(err);
}


void WASI::FdRead(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t iovs_ptr;
  uint32_t iovs_len;
  uint32_t nread_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 4);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, iovs_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, iovs_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, nread_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_read(%d, %d, %d, %d)\n", fd, iovs_ptr, iovs_len, nread_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, iovs_ptr, iovs_len * 8);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, nread_ptr, 4);
  uvwasi_iovec_t* iovs = UncheckedCalloc<uvwasi_iovec_t>(iovs_len);

  if (iovs == nullptr) {
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  for (uint32_t i = 0; i < iovs_len; ++i) {
    uint32_t buf_ptr;
    uint32_t buf_len;

    wasi->readUInt32(memory, &buf_ptr, iovs_ptr);
    wasi->readUInt32(memory, &buf_len, iovs_ptr + 4);

    if (is_access_oob(mem_size, buf_ptr, buf_len)) {
      free(iovs);
      args.GetReturnValue().Set(UVWASI_EOVERFLOW);
      return;
    }

    iovs_ptr += 8;
    iovs[i].buf = static_cast<void*>(&memory[buf_ptr]);
    iovs[i].buf_len = buf_len;
  }

  size_t nread;
  uvwasi_errno_t err = uvwasi_fd_read(&wasi->uvw_,
                                      fd,
                                      iovs,
                                      iovs_len,
                                      &nread);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, nread, nread_ptr);

  free(iovs);
  args.GetReturnValue().Set(err);
}


void WASI::FdReaddir(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t buf_ptr;
  uint32_t buf_len;
  uint64_t cookie;
  uint32_t bufused_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 5);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, buf_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, buf_len);
  UNWRAP_BIGINT_OR_RETURN(args, args[3], Uint64, cookie);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, bufused_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "uvwasi_fd_readdir(%d, %d, %d, %d, %d)\n",
        fd,
        buf_ptr,
        buf_len,
        cookie,
        bufused_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, buf_ptr, buf_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, bufused_ptr, 4);
  size_t bufused;
  uvwasi_errno_t err = uvwasi_fd_readdir(&wasi->uvw_,
                                         fd,
                                         &memory[buf_ptr],
                                         buf_len,
                                         cookie,
                                         &bufused);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, bufused, bufused_ptr);

  args.GetReturnValue().Set(err);
}


void WASI::FdRenumber(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t from;
  uint32_t to;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, from);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, to);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_renumber(%d, %d)\n", from, to);
  uvwasi_errno_t err = uvwasi_fd_renumber(&wasi->uvw_, from, to);
  args.GetReturnValue().Set(err);
}


void WASI::FdSeek(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  int64_t offset;
  uint8_t whence;
  uint32_t newoffset_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 4);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  UNWRAP_BIGINT_OR_RETURN(args, args[1], Int64, offset);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, whence);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, newoffset_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_seek(%d, %d, %d, %d)\n", fd, offset, whence, newoffset_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, newoffset_ptr, 8);
  uvwasi_filesize_t newoffset;
  uvwasi_errno_t err = uvwasi_fd_seek(&wasi->uvw_,
                                      fd,
                                      offset,
                                      whence,
                                      &newoffset);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt64(memory, newoffset, newoffset_ptr);

  args.GetReturnValue().Set(err);
}


void WASI::FdSync(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  RETURN_IF_BAD_ARG_COUNT(args, 1);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_sync(%d)\n", fd);
  uvwasi_errno_t err = uvwasi_fd_sync(&wasi->uvw_, fd);
  args.GetReturnValue().Set(err);
}


void WASI::FdTell(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t offset_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, offset_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "fd_tell(%d, %d)\n", fd, offset_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, offset_ptr, 8);
  uvwasi_filesize_t offset;
  uvwasi_errno_t err = uvwasi_fd_tell(&wasi->uvw_, fd, &offset);

  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt64(memory, offset, offset_ptr);

  args.GetReturnValue().Set(err);
}


void WASI::FdWrite(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t iovs_ptr;
  uint32_t iovs_len;
  uint32_t nwritten_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 4);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, iovs_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, iovs_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, nwritten_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "fd_write(%d, %d, %d, %d)\n",
        fd,
        iovs_ptr,
        iovs_len,
        nwritten_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, iovs_ptr, iovs_len * 8);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, nwritten_ptr, 4);
  uvwasi_ciovec_t* iovs = UncheckedCalloc<uvwasi_ciovec_t>(iovs_len);

  if (iovs == nullptr) {
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  for (uint32_t i = 0; i < iovs_len; ++i) {
    uint32_t buf_ptr;
    uint32_t buf_len;

    wasi->readUInt32(memory, &buf_ptr, iovs_ptr);
    wasi->readUInt32(memory, &buf_len, iovs_ptr + 4);

    if (is_access_oob(mem_size, buf_ptr, buf_len)) {
      free(iovs);
      args.GetReturnValue().Set(UVWASI_EOVERFLOW);
      return;
    }

    iovs_ptr += 8;
    iovs[i].buf = static_cast<void*>(&memory[buf_ptr]);
    iovs[i].buf_len = buf_len;
  }

  size_t nwritten;
  uvwasi_errno_t err = uvwasi_fd_write(&wasi->uvw_,
                                       fd,
                                       iovs,
                                       iovs_len,
                                       &nwritten);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, nwritten, nwritten_ptr);

  free(iovs);
  args.GetReturnValue().Set(err);
}


void WASI::PathCreateDirectory(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t path_ptr;
  uint32_t path_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 3);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "path_create_directory(%d, %d, %d)\n", fd, path_ptr, path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  uvwasi_errno_t err = uvwasi_path_create_directory(&wasi->uvw_,
                                                    fd,
                                                    &memory[path_ptr],
                                                    path_len);
  args.GetReturnValue().Set(err);
}


void WASI::PathFilestatGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t flags;
  uint32_t path_ptr;
  uint32_t path_len;
  uint32_t buf_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 5);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, flags);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, path_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, buf_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "path_filestat_get(%d, %d, %d)\n",
        fd,
        path_ptr,
        path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, buf_ptr, 64);
  uvwasi_filestat_t stats;
  uvwasi_errno_t err = uvwasi_path_filestat_get(&wasi->uvw_,
                                                fd,
                                                flags,
                                                &memory[path_ptr],
                                                path_len,
                                                &stats);
  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt64(memory, stats.st_dev, buf_ptr);
    wasi->writeUInt64(memory, stats.st_ino, buf_ptr + 8);
    wasi->writeUInt8(memory, stats.st_filetype, buf_ptr + 16);
    wasi->writeUInt64(memory, stats.st_nlink, buf_ptr + 24);
    wasi->writeUInt64(memory, stats.st_size, buf_ptr + 32);
    wasi->writeUInt64(memory, stats.st_atim, buf_ptr + 40);
    wasi->writeUInt64(memory, stats.st_mtim, buf_ptr + 48);
    wasi->writeUInt64(memory, stats.st_ctim, buf_ptr + 56);
  }

  args.GetReturnValue().Set(err);
}


void WASI::PathFilestatSetTimes(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t flags;
  uint32_t path_ptr;
  uint32_t path_len;
  uint64_t st_atim;
  uint64_t st_mtim;
  uint16_t fst_flags;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 7);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, flags);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, path_len);
  UNWRAP_BIGINT_OR_RETURN(args, args[4], Uint64, st_atim);
  UNWRAP_BIGINT_OR_RETURN(args, args[5], Uint64, st_mtim);
  CHECK_TO_TYPE_OR_RETURN(args, args[6], Uint32, fst_flags);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "path_filestat_set_times(%d, %d, %d, %d, %d, %d, %d)\n",
        fd,
        flags,
        path_ptr,
        path_len,
        st_atim,
        st_mtim,
        fst_flags);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  uvwasi_errno_t err = uvwasi_path_filestat_set_times(&wasi->uvw_,
                                                      fd,
                                                      flags,
                                                      &memory[path_ptr],
                                                      path_len,
                                                      st_atim,
                                                      st_mtim,
                                                      fst_flags);
  args.GetReturnValue().Set(err);
}


void WASI::PathLink(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t old_fd;
  uint32_t old_flags;
  uint32_t old_path_ptr;
  uint32_t old_path_len;
  uint32_t new_fd;
  uint32_t new_path_ptr;
  uint32_t new_path_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 7);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, old_fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, old_flags);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, old_path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, old_path_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, new_fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[5], Uint32, new_path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[6], Uint32, new_path_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "path_link(%d, %d, %d, %d, %d, %d, %d)\n",
        old_fd,
        old_flags,
        old_path_ptr,
        old_path_len,
        new_fd,
        new_path_ptr,
        new_path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, old_path_ptr, old_path_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, new_path_ptr, new_path_len);
  uvwasi_errno_t err = uvwasi_path_link(&wasi->uvw_,
                                        old_fd,
                                        old_flags,
                                        &memory[old_path_ptr],
                                        old_path_len,
                                        new_fd,
                                        &memory[new_path_ptr],
                                        new_path_len);
  args.GetReturnValue().Set(err);
}


void WASI::PathOpen(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t dirfd;
  uint32_t dirflags;
  uint32_t path_ptr;
  uint32_t path_len;
  uint32_t o_flags;
  uint64_t fs_rights_base;
  uint64_t fs_rights_inheriting;
  uint32_t fs_flags;
  uint32_t fd_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 9);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, dirfd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, dirflags);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, path_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, o_flags);
  UNWRAP_BIGINT_OR_RETURN(args, args[5], Uint64, fs_rights_base);
  UNWRAP_BIGINT_OR_RETURN(args, args[6], Uint64, fs_rights_inheriting);
  CHECK_TO_TYPE_OR_RETURN(args, args[7], Uint32, fs_flags);
  CHECK_TO_TYPE_OR_RETURN(args, args[8], Uint32, fd_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "path_open(%d, %d, %d, %d, %d, %d, %d, %d, %d)\n",
        dirfd,
        dirflags,
        path_ptr,
        path_len,
        o_flags,
        fs_rights_base,
        fs_rights_inheriting,
        fs_flags,
        fd_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, fd_ptr, 4);
  uvwasi_fd_t fd;
  uvwasi_errno_t err = uvwasi_path_open(&wasi->uvw_,
                                        dirfd,
                                        dirflags,
                                        &memory[path_ptr],
                                        path_len,
                                        static_cast<uvwasi_oflags_t>(o_flags),
                                        fs_rights_base,
                                        fs_rights_inheriting,
                                        static_cast<uvwasi_fdflags_t>(fs_flags),
                                        &fd);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, fd, fd_ptr);

  args.GetReturnValue().Set(err);
}


void WASI::PathReadlink(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t path_ptr;
  uint32_t path_len;
  uint32_t buf_ptr;
  uint32_t buf_len;
  uint32_t bufused_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 6);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, buf_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, buf_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[5], Uint32, bufused_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "path_readlink(%d, %d, %d, %d, %d, %d)\n",
        fd,
        path_ptr,
        path_len,
        buf_ptr,
        buf_len,
        bufused_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, buf_ptr, buf_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, bufused_ptr, 4);
  size_t bufused;
  uvwasi_errno_t err = uvwasi_path_readlink(&wasi->uvw_,
                                        fd,
                                        &memory[path_ptr],
                                        path_len,
                                        &memory[buf_ptr],
                                        buf_len,
                                        &bufused);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, bufused, bufused_ptr);

  args.GetReturnValue().Set(err);
}


void WASI::PathRemoveDirectory(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t path_ptr;
  uint32_t path_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 3);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "path_remove_directory(%d, %d, %d)\n", fd, path_ptr, path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  uvwasi_errno_t err = uvwasi_path_remove_directory(&wasi->uvw_,
                                                    fd,
                                                    &memory[path_ptr],
                                                    path_len);
  args.GetReturnValue().Set(err);
}


void WASI::PathRename(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t old_fd;
  uint32_t old_path_ptr;
  uint32_t old_path_len;
  uint32_t new_fd;
  uint32_t new_path_ptr;
  uint32_t new_path_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 6);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, old_fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, old_path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, old_path_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, new_fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, new_path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[5], Uint32, new_path_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "path_rename(%d, %d, %d, %d, %d, %d)\n",
        old_fd,
        old_path_ptr,
        old_path_len,
        new_fd,
        new_path_ptr,
        new_path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, old_path_ptr, old_path_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, new_path_ptr, new_path_len);
  uvwasi_errno_t err = uvwasi_path_rename(&wasi->uvw_,
                                          old_fd,
                                          &memory[old_path_ptr],
                                          old_path_len,
                                          new_fd,
                                          &memory[new_path_ptr],
                                          new_path_len);
  args.GetReturnValue().Set(err);
}


void WASI::PathSymlink(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t old_path_ptr;
  uint32_t old_path_len;
  uint32_t fd;
  uint32_t new_path_ptr;
  uint32_t new_path_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 5);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, old_path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, old_path_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, new_path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, new_path_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "path_symlink(%d, %d, %d, %d, %d)\n",
        old_path_ptr,
        old_path_len,
        fd,
        new_path_ptr,
        new_path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, old_path_ptr, old_path_len);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, new_path_ptr, new_path_len);
  uvwasi_errno_t err = uvwasi_path_symlink(&wasi->uvw_,
                                           &memory[old_path_ptr],
                                           old_path_len,
                                           fd,
                                           &memory[new_path_ptr],
                                           new_path_len);
  args.GetReturnValue().Set(err);
}


void WASI::PathUnlinkFile(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t fd;
  uint32_t path_ptr;
  uint32_t path_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 3);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, fd);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, path_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, path_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "path_unlink_file(%d, %d, %d)\n", fd, path_ptr, path_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, path_ptr, path_len);
  uvwasi_errno_t err = uvwasi_path_unlink_file(&wasi->uvw_,
                                               fd,
                                               &memory[path_ptr],
                                               path_len);
  args.GetReturnValue().Set(err);
}


void WASI::PollOneoff(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t in_ptr;
  uint32_t out_ptr;
  uint32_t nsubscriptions;
  uint32_t nevents_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 4);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, in_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, out_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, nsubscriptions);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, nevents_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "poll_oneoff(%d, %d, %d, %d)\n",
        in_ptr,
        out_ptr,
        nsubscriptions,
        nevents_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, in_ptr, nsubscriptions * 48);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, out_ptr, nsubscriptions * 32);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, nevents_ptr, 4);
  uvwasi_subscription_t* in =
      UncheckedCalloc<uvwasi_subscription_t>(nsubscriptions);

  if (in == nullptr) {
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  uvwasi_event_t* out = UncheckedCalloc<uvwasi_event_t>(nsubscriptions);

  if (out == nullptr) {
    free(in);
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  for (uint32_t i = 0; i < nsubscriptions; ++i) {
    uvwasi_subscription_t sub = in[i];
    wasi->readUInt64(memory, &sub.userdata, in_ptr);
    wasi->readUInt8(memory, &sub.type, in_ptr + 8);

    if (sub.type == UVWASI_EVENTTYPE_CLOCK) {
      wasi->readUInt32(memory, &sub.u.clock.clock_id, in_ptr + 16);
      wasi->readUInt64(memory, &sub.u.clock.timeout, in_ptr + 24);
      wasi->readUInt64(memory, &sub.u.clock.precision, in_ptr + 32);
      wasi->readUInt16(memory, &sub.u.clock.flags, in_ptr + 40);
    } else if (sub.type == UVWASI_EVENTTYPE_FD_READ ||
               sub.type == UVWASI_EVENTTYPE_FD_WRITE) {
      wasi->readUInt32(memory, &sub.u.fd_readwrite.fd, in_ptr + 16);
    }

    in_ptr += 56;
  }

  size_t nevents;
  uvwasi_errno_t err = uvwasi_poll_oneoff(&wasi->uvw_,
                                          in,
                                          out,
                                          nsubscriptions,
                                          &nevents);
  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt32(memory, nevents, nevents_ptr);

    for (uint32_t i = 0; i < nsubscriptions; ++i) {
      uvwasi_event_t event = out[i];

      wasi->writeUInt64(memory, event.userdata, out_ptr);
      wasi->writeUInt16(memory, event.error, out_ptr + 8);
      wasi->writeUInt8(memory, event.type, out_ptr + 10);

      if (event.type == UVWASI_EVENTTYPE_FD_READ ||
          event.type == UVWASI_EVENTTYPE_FD_WRITE) {
        wasi->writeUInt64(memory, event.u.fd_readwrite.nbytes, out_ptr + 16);
        wasi->writeUInt16(memory, event.u.fd_readwrite.flags, out_ptr + 24);
      }

      out_ptr += 32;
    }
  }

  free(in);
  free(out);
  args.GetReturnValue().Set(err);
}


void WASI::ProcExit(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t code;
  RETURN_IF_BAD_ARG_COUNT(args, 1);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, code);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "proc_exit(%d)\n", code);
  args.GetReturnValue().Set(uvwasi_proc_exit(&wasi->uvw_, code));
}


void WASI::ProcRaise(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t sig;
  RETURN_IF_BAD_ARG_COUNT(args, 1);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, sig);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "proc_raise(%d)\n", sig);
  uvwasi_errno_t err = uvwasi_proc_raise(&wasi->uvw_, sig);
  args.GetReturnValue().Set(err);
}


void WASI::RandomGet(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t buf_ptr;
  uint32_t buf_len;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, buf_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, buf_len);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "random_get(%d, %d)\n", buf_ptr, buf_len);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, buf_ptr, buf_len);
  uvwasi_errno_t err = uvwasi_random_get(&wasi->uvw_,
                                         &memory[buf_ptr],
                                         buf_len);
  args.GetReturnValue().Set(err);
}


void WASI::SchedYield(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  RETURN_IF_BAD_ARG_COUNT(args, 0);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "sched_yield()\n");
  uvwasi_errno_t err = uvwasi_sched_yield(&wasi->uvw_);
  args.GetReturnValue().Set(err);
}


void WASI::SockRecv(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t sock;
  uint32_t ri_data_ptr;
  uint32_t ri_data_len;
  uint16_t ri_flags;
  uint32_t ro_datalen_ptr;
  uint16_t ro_flags_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 6);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, sock);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, ri_data_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, ri_data_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, ri_flags);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, ro_datalen_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[5], Uint32, ro_flags_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "sock_recv(%d, %d, %d, %d, %d, %d)\n",
        sock,
        ri_data_ptr,
        ri_data_len,
        ri_flags,
        ro_datalen_ptr,
        ro_flags_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, ri_data_ptr, ri_data_len * 8);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, ro_datalen_ptr, 4);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, ro_flags_ptr, 4);
  uvwasi_iovec_t* ri_data = UncheckedCalloc<uvwasi_iovec_t>(ri_data_len);

  if (ri_data == nullptr) {
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  for (uint32_t i = 0; i < ri_data_len; ++i) {
    uint32_t buf_ptr;
    uint32_t buf_len;

    wasi->readUInt32(memory, &buf_ptr, ri_data_ptr);
    wasi->readUInt32(memory, &buf_len, ri_data_ptr + 4);

    if (is_access_oob(mem_size, buf_ptr, buf_len)) {
      free(ri_data);
      args.GetReturnValue().Set(UVWASI_EOVERFLOW);
      return;
    }

    ri_data_ptr += 8;
    ri_data[i].buf = static_cast<void*>(&memory[buf_ptr]);
    ri_data[i].buf_len = buf_len;
  }

  size_t ro_datalen;
  uvwasi_roflags_t ro_flags;
  uvwasi_errno_t err = uvwasi_sock_recv(&wasi->uvw_,
                                        sock,
                                        ri_data,
                                        ri_data_len,
                                        ri_flags,
                                        &ro_datalen,
                                        &ro_flags);
  if (err == UVWASI_ESUCCESS) {
    wasi->writeUInt32(memory, ro_datalen, ro_datalen_ptr);
    wasi->writeUInt32(memory, ro_flags, ro_flags_ptr);
  }

  free(ri_data);
  args.GetReturnValue().Set(err);
}


void WASI::SockSend(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t sock;
  uint32_t si_data_ptr;
  uint32_t si_data_len;
  uint16_t si_flags;
  uint32_t so_datalen_ptr;
  char* memory;
  size_t mem_size;
  RETURN_IF_BAD_ARG_COUNT(args, 5);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, sock);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, si_data_ptr);
  CHECK_TO_TYPE_OR_RETURN(args, args[2], Uint32, si_data_len);
  CHECK_TO_TYPE_OR_RETURN(args, args[3], Uint32, si_flags);
  CHECK_TO_TYPE_OR_RETURN(args, args[4], Uint32, so_datalen_ptr);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi,
        "sock_send(%d, %d, %d, %d, %d)\n",
        sock,
        si_data_ptr,
        si_data_len,
        si_flags,
        so_datalen_ptr);
  GET_BACKING_STORE_OR_RETURN(wasi, args, &memory, &mem_size);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, si_data_ptr, si_data_len * 8);
  CHECK_BOUNDS_OR_RETURN(args, mem_size, so_datalen_ptr, 4);
  uvwasi_ciovec_t* si_data = UncheckedCalloc<uvwasi_ciovec_t>(si_data_len);

  if (si_data == nullptr) {
    args.GetReturnValue().Set(UVWASI_ENOMEM);
    return;
  }

  for (uint32_t i = 0; i < si_data_len; ++i) {
    uint32_t buf_ptr;
    uint32_t buf_len;

    wasi->readUInt32(memory, &buf_ptr, si_data_ptr);
    wasi->readUInt32(memory, &buf_len, si_data_ptr + 4);

    if (is_access_oob(mem_size, buf_ptr, buf_len)) {
      free(si_data);
      args.GetReturnValue().Set(UVWASI_EOVERFLOW);
      return;
    }

    si_data_ptr += 8;
    si_data[i].buf = static_cast<void*>(&memory[buf_ptr]);
    si_data[i].buf_len = buf_len;
  }

  size_t so_datalen;
  uvwasi_errno_t err = uvwasi_sock_send(&wasi->uvw_,
                                        sock,
                                        si_data,
                                        si_data_len,
                                        si_flags,
                                        &so_datalen);
  if (err == UVWASI_ESUCCESS)
    wasi->writeUInt32(memory, so_datalen, so_datalen_ptr);

  free(si_data);
  args.GetReturnValue().Set(err);
}


void WASI::SockShutdown(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  uint32_t sock;
  uint8_t how;
  RETURN_IF_BAD_ARG_COUNT(args, 2);
  CHECK_TO_TYPE_OR_RETURN(args, args[0], Uint32, sock);
  CHECK_TO_TYPE_OR_RETURN(args, args[1], Uint32, how);
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  Debug(wasi, "sock_shutdown(%d, %d)\n", sock, how);
  uvwasi_errno_t err = uvwasi_sock_shutdown(&wasi->uvw_, sock, how);
  args.GetReturnValue().Set(err);
}


void WASI::_SetMemory(const FunctionCallbackInfo<Value>& args) {
  WASI* wasi;
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsObject());
  ASSIGN_OR_RETURN_UNWRAP(&wasi, args.This());
  wasi->memory_.Reset(wasi->env()->isolate(), args[0].As<Object>());
}


void WASI::readUInt8(char* memory, uint8_t* value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  CHECK_NOT_NULL(value);
  *value = memory[offset] & 0xFF;
}


void WASI::readUInt16(char* memory, uint16_t* value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  CHECK_NOT_NULL(value);
  *value = (memory[offset] & 0xFF) |
           ((memory[offset + 1] & 0xFF) << 8);
}


void WASI::readUInt32(char* memory, uint32_t* value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  CHECK_NOT_NULL(value);
  *value = (memory[offset] & 0xFF) |
           ((memory[offset + 1] & 0xFF) << 8) |
           ((memory[offset + 2] & 0xFF) << 16) |
           ((memory[offset + 3] & 0xFF) << 24);
}


void WASI::readUInt64(char* memory, uint64_t* value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  CHECK_NOT_NULL(value);
  uint64_t low = (memory[offset] & 0xFF) |
                 ((memory[offset + 1] & 0xFF) << 8) |
                 ((memory[offset + 2] & 0xFF) << 16) |
                 ((memory[offset + 3] & 0xFF) << 24);
  uint64_t high = (memory[offset + 4] & 0xFF) |
                  ((memory[offset + 5] & 0xFF) << 8) |
                  ((memory[offset + 6] & 0xFF) << 16) |
                  ((memory[offset + 7] & 0xFF) << 24);
  *value = (high << 32) + low;
}


void WASI::writeUInt8(char* memory, uint8_t value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  memory[offset] = value & 0xFF;
}


void WASI::writeUInt16(char* memory, uint16_t value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  memory[offset++] = value & 0xFF;
  memory[offset] = (value >> 8) & 0xFF;
}


void WASI::writeUInt32(char* memory, uint32_t value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  memory[offset++] = value & 0xFF;
  memory[offset++] = (value >> 8) & 0xFF;
  memory[offset++] = (value >> 16) & 0xFF;
  memory[offset] = (value >> 24) & 0xFF;
}


void WASI::writeUInt64(char* memory, uint64_t value, uint32_t offset) {
  CHECK_NOT_NULL(memory);
  memory[offset++] = value & 0xFF;
  memory[offset++] = (value >> 8) & 0xFF;
  memory[offset++] = (value >> 16) & 0xFF;
  memory[offset++] = (value >> 24) & 0xFF;
  memory[offset++] = (value >> 32) & 0xFF;
  memory[offset++] = (value >> 40) & 0xFF;
  memory[offset++] = (value >> 48) & 0xFF;
  memory[offset] = (value >> 56) & 0xFF;
}


uvwasi_errno_t WASI::backingStore(char** store, size_t* byte_length) {
  Environment* env = this->env();
  Local<Object> memory = PersistentToLocal::Strong(this->memory_);
  Local<Value> prop;

  if (!memory->Get(env->context(), env->buffer_string()).ToLocal(&prop))
    return UVWASI_EINVAL;

  if (!prop->IsArrayBuffer())
    return UVWASI_EINVAL;

  Local<ArrayBuffer> ab = prop.As<ArrayBuffer>();
  std::shared_ptr<BackingStore> backing_store = ab->GetBackingStore();
  *byte_length = backing_store->ByteLength();
  *store = static_cast<char*>(backing_store->Data());
  return UVWASI_ESUCCESS;
}


static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);

  Local<FunctionTemplate> tmpl = env->NewFunctionTemplate(WASI::New);
  auto wasi_wrap_string = FIXED_ONE_BYTE_STRING(env->isolate(), "WASI");
  tmpl->InstanceTemplate()->SetInternalFieldCount(WASI::kInternalFieldCount);
  tmpl->SetClassName(wasi_wrap_string);

  env->SetProtoMethod(tmpl, "args_get", WASI::ArgsGet);
  env->SetProtoMethod(tmpl, "args_sizes_get", WASI::ArgsSizesGet);
  env->SetProtoMethod(tmpl, "clock_res_get", WASI::ClockResGet);
  env->SetProtoMethod(tmpl, "clock_time_get", WASI::ClockTimeGet);
  env->SetProtoMethod(tmpl, "environ_get", WASI::EnvironGet);
  env->SetProtoMethod(tmpl, "environ_sizes_get", WASI::EnvironSizesGet);
  env->SetProtoMethod(tmpl, "fd_advise", WASI::FdAdvise);
  env->SetProtoMethod(tmpl, "fd_allocate", WASI::FdAllocate);
  env->SetProtoMethod(tmpl, "fd_close", WASI::FdClose);
  env->SetProtoMethod(tmpl, "fd_datasync", WASI::FdDatasync);
  env->SetProtoMethod(tmpl, "fd_fdstat_get", WASI::FdFdstatGet);
  env->SetProtoMethod(tmpl, "fd_fdstat_set_flags", WASI::FdFdstatSetFlags);
  env->SetProtoMethod(tmpl, "fd_fdstat_set_rights", WASI::FdFdstatSetRights);
  env->SetProtoMethod(tmpl, "fd_filestat_get", WASI::FdFilestatGet);
  env->SetProtoMethod(tmpl, "fd_filestat_set_size", WASI::FdFilestatSetSize);
  env->SetProtoMethod(tmpl, "fd_filestat_set_times", WASI::FdFilestatSetTimes);
  env->SetProtoMethod(tmpl, "fd_pread", WASI::FdPread);
  env->SetProtoMethod(tmpl, "fd_prestat_get", WASI::FdPrestatGet);
  env->SetProtoMethod(tmpl, "fd_prestat_dir_name", WASI::FdPrestatDirName);
  env->SetProtoMethod(tmpl, "fd_pwrite", WASI::FdPwrite);
  env->SetProtoMethod(tmpl, "fd_read", WASI::FdRead);
  env->SetProtoMethod(tmpl, "fd_readdir", WASI::FdReaddir);
  env->SetProtoMethod(tmpl, "fd_renumber", WASI::FdRenumber);
  env->SetProtoMethod(tmpl, "fd_seek", WASI::FdSeek);
  env->SetProtoMethod(tmpl, "fd_sync", WASI::FdSync);
  env->SetProtoMethod(tmpl, "fd_tell", WASI::FdTell);
  env->SetProtoMethod(tmpl, "fd_write", WASI::FdWrite);
  env->SetProtoMethod(tmpl, "path_create_directory", WASI::PathCreateDirectory);
  env->SetProtoMethod(tmpl, "path_filestat_get", WASI::PathFilestatGet);
  env->SetProtoMethod(tmpl,
                      "path_filestat_set_times",
                      WASI::PathFilestatSetTimes);
  env->SetProtoMethod(tmpl, "path_link", WASI::PathLink);
  env->SetProtoMethod(tmpl, "path_open", WASI::PathOpen);
  env->SetProtoMethod(tmpl, "path_readlink", WASI::PathReadlink);
  env->SetProtoMethod(tmpl, "path_remove_directory", WASI::PathRemoveDirectory);
  env->SetProtoMethod(tmpl, "path_rename", WASI::PathRename);
  env->SetProtoMethod(tmpl, "path_symlink", WASI::PathSymlink);
  env->SetProtoMethod(tmpl, "path_unlink_file", WASI::PathUnlinkFile);
  env->SetProtoMethod(tmpl, "poll_oneoff", WASI::PollOneoff);
  env->SetProtoMethod(tmpl, "proc_exit", WASI::ProcExit);
  env->SetProtoMethod(tmpl, "proc_raise", WASI::ProcRaise);
  env->SetProtoMethod(tmpl, "random_get", WASI::RandomGet);
  env->SetProtoMethod(tmpl, "sched_yield", WASI::SchedYield);
  env->SetProtoMethod(tmpl, "sock_recv", WASI::SockRecv);
  env->SetProtoMethod(tmpl, "sock_send", WASI::SockSend);
  env->SetProtoMethod(tmpl, "sock_shutdown", WASI::SockShutdown);

  env->SetInstanceMethod(tmpl, "_setMemory", WASI::_SetMemory);

  target->Set(env->context(),
              wasi_wrap_string,
              tmpl->GetFunction(context).ToLocalChecked()).ToChecked();
}


}  // namespace wasi
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(wasi, node::wasi::Initialize)
