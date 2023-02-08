#include "node_sea.h"

#include "env-inl.h"
#include "node_internals.h"
#include "v8-local-handle.h"
#include "v8-primitive.h"
#include "v8-value.h"

#define POSTJECT_SENTINEL_FUSE "NODE_JS_FUSE_fce680ab2cc467b6e072b8b5df1996b2"
#include "postject-api.h"

#include <tuple>

#if !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)

using v8::Local;
using v8::MaybeLocal;
using v8::String;
using v8::Value;

namespace {

bool single_executable_application_code_loaded = false;
size_t single_executable_application_size = 0;
const char* single_executable_application_code = nullptr;

const char* FindSingleExecutableCode(size_t* size) {
  if (single_executable_application_code_loaded == false) {
#ifdef __APPLE__
    postject_options options;
    postject_options_init(&options);
    options.macho_segment_name = "NODE_JS";
    single_executable_application_code =
        static_cast<const char*>(postject_find_resource(
            "NODE_JS_CODE", &single_executable_application_size, &options));
#else
    single_executable_application_code =
        static_cast<const char*>(postject_find_resource(
            "NODE_JS_CODE", &single_executable_application_size, nullptr));
#endif
    single_executable_application_code_loaded = true;
  }

  if (size != nullptr) {
    *size = single_executable_application_size;
  }

  return single_executable_application_code;
}

class ExternalOneByteStringSingleExecutableCode
    : public String::ExternalOneByteStringResource {
 public:
  explicit ExternalOneByteStringSingleExecutableCode(const char* data,
                                                     size_t size)
      : data_(data), size_(size) {}

  const char* data() const override { return data_; }

  size_t length() const override { return size_; }

 private:
  const char* data_;
  size_t size_;
};

}  // namespace

namespace node {
namespace per_process {
namespace sea {

bool IsSingleExecutable() {
  return postject_has_resource();
}

MaybeLocal<Value> StartSingleExecutableExecution(Environment* env) {
  size_t size = 0;
  // TODO(RaisinTen): Add support for non-ASCII character inputs.
  const char* code = FindSingleExecutableCode(&size);

  if (code == nullptr) {
    return {};
  }

  Local<String> code_external_string =
      String::NewExternalOneByte(
          env->isolate(),
          new ExternalOneByteStringSingleExecutableCode(code, size))
          .ToLocalChecked();

  env->process_object()
      ->SetPrivate(env->context(),
                   env->single_executable_application_code(),
                   code_external_string)
      .Check();

  return StartExecution(env, "internal/main/single_executable_application");
}

std::tuple<int, char**> FixupArgsForSEA(int argc, char** argv) {
  // Repeats argv[0] at position 1 on argv as a replacement for the missing
  // entry point file path.
  if (IsSingleExecutable()) {
    char** new_argv = new char*[argc + 2];
    int new_argc = 0;
    new_argv[new_argc++] = argv[0];
    new_argv[new_argc++] = argv[0];

    for (int i = 1; i < argc; ++i) {
      new_argv[new_argc++] = argv[i];
    }

    new_argv[new_argc] = nullptr;

    argc = new_argc;
    argv = new_argv;
  }

  return {argc, argv};
}

}  // namespace sea
}  // namespace per_process
}  // namespace node

#endif  // !defined(DISABLE_SINGLE_EXECUTABLE_APPLICATION)
