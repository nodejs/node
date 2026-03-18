#include "typescript_transpiler.h"

#include <fcntl.h>
#include <memory>
#include <string>
#include <string_view>

#include "libplatform/libplatform.h"
#include "uv.h"
#include "v8.h"

namespace node {
namespace js2c {

namespace {

using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::MaybeLocal;
using v8::Object;
using v8::Platform;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::TryCatch;
using v8::V8;
using v8::Value;

constexpr std::string_view kAmaroPath = "deps/amaro/dist/index.js";

constexpr char kBootstrapScript[] = R"JS((function(globalThis) {
  function encodeUtf8(input) {
    return Uint8Array.from(
      unescape(encodeURIComponent(String(input))),
      (char) => char.charCodeAt(0));
  }

  function decodeUtf8(input) {
    let binary = '';
    const chunkSize = 0x8000;
    for (let i = 0; i < input.length; i += chunkSize) {
      binary += String.fromCharCode.apply(
        String,
        input.subarray(i, i + chunkSize));
    }
    return decodeURIComponent(escape(binary));
  }

  class TextEncoder {
    encode(input = '') {
      return encodeUtf8(input);
    }
  }

  class TextDecoder {
    constructor(encoding = 'utf-8', options = {}) {
      if (encoding !== 'utf-8' || !options.ignoreBOM) {
        throw new Error('Unexpected TextDecoder usage');
      }
    }

    decode(input = new Uint8Array(), options = undefined) {
      if (!(input instanceof Uint8Array) || options !== undefined) {
        throw new Error('Unexpected TextDecoder usage');
      }
      return decodeUtf8(input);
    }
  }

  const Buffer = {
    from(value, encoding) {
      if (encoding === 'base64') {
        return Uint8Array.fromBase64(String(value));
      }
      throw new Error(`Unsupported Buffer.from encoding: ${encoding}`);
    }
  };

  globalThis.require = (id) => {
    if (id === 'util') {
      return { TextDecoder, TextEncoder };
    }
    if (id === 'node:buffer') {
      return { Buffer };
    }
    throw new Error(`Unsupported require: ${id}`);
  };

  globalThis.module = { exports: {} };
})(globalThis);
)JS";

MaybeLocal<String> ReadUtf8String(Isolate* isolate, std::string_view source) {
  return String::NewFromUtf8(isolate,
                             source.data(),
                             v8::NewStringType::kNormal,
                             static_cast<int>(source.size()));
}

std::string ToUtf8(Isolate* isolate, Local<Value> value) {
  String::Utf8Value string(isolate, value);
  if (*string == nullptr) {
    return {};
  }
  return std::string(*string, string.length());
}

std::string FormatException(Isolate* isolate, TryCatch* try_catch) {
  std::string message = ToUtf8(isolate, try_catch->Exception());
  Local<Message> exception_message = try_catch->Message();
  if (exception_message.IsEmpty()) {
    return message.empty() ? "Unknown exception" : message;
  }
  std::string location = ToUtf8(isolate, exception_message->Get());
  if (!location.empty()) {
    return location;
  }
  return message.empty() ? "Unknown exception" : message;
}

int CloseFile(uv_file file) {
  uv_fs_t req;
  int err = uv_fs_close(nullptr, &req, file, nullptr);
  uv_fs_req_cleanup(&req);
  return err;
}

}  // namespace

class TypeScriptTranspiler::Impl {
 public:
  Impl() = default;

  ~Impl() {
    transform_sync_.Reset();
    context_.Reset();
    if (isolate_ != nullptr) {
      isolate_->Dispose();
    }
    if (v8_initialized_) {
      v8::V8::Dispose();
    }
    if (platform_initialized_) {
      v8::V8::DisposePlatform();
    }
  }

  int Initialize(const char* argv0) {
    if (isolate_ != nullptr) {
      return 0;
    }

    last_error_.clear();

    V8::InitializeICUDefaultLocation(argv0);
    V8::InitializeExternalStartupData(argv0);
    platform_ = v8::platform::NewDefaultPlatform();
    V8::InitializePlatform(platform_.get());
    platform_initialized_ = true;
    V8::Initialize();
    v8_initialized_ = true;

    allocator_.reset(ArrayBuffer::Allocator::NewDefaultAllocator());
    Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = allocator_.get();
    isolate_ = Isolate::New(create_params);

    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    context_.Reset(isolate_, context);
    Context::Scope context_scope(context);

    int r = RunScript("js2c-amaro-bootstrap", kBootstrapScript);
    if (r != 0) {
      return r;
    }

    std::string source;
    if (!ReadFile(kAmaroPath, &source)) {
      last_error_ = "Failed to read " + std::string(kAmaroPath);
      return 1;
    }
    r = RunScript(kAmaroPath, source);
    if (r != 0) {
      return r;
    }

    Local<Object> global = context->Global();
    Local<Value> module_value;
    if (!global->Get(context, ReadOnlyString("module")).ToLocal(&module_value) ||
        !module_value->IsObject()) {
      last_error_ = "Failed to load amaro module";
      return 1;
    }

    Local<Object> module_object = module_value.As<Object>();
    Local<Value> exports_value;
    if (!module_object->Get(context, ReadOnlyString("exports")).ToLocal(&exports_value) ||
        !exports_value->IsObject()) {
      last_error_ = "Failed to access amaro exports";
      return 1;
    }

    Local<Object> exports_object = exports_value.As<Object>();
    Local<Value> transform_value;
    if (!exports_object->Get(context, ReadOnlyString("transformSync")).ToLocal(&transform_value) ||
        !transform_value->IsFunction()) {
      last_error_ = "Failed to find amaro transformSync";
      return 1;
    }

    transform_sync_.Reset(isolate_, transform_value.As<Function>());
    return 0;
  }

  int Strip(std::string_view source,
            std::string_view filename,
            std::vector<char>* output) {
    if (isolate_ == nullptr) {
      last_error_ = "TypeScript transpiler is not initialized";
      return 1;
    }

    last_error_.clear();

    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);
    Local<Context> context = context_.Get(isolate_);
    Context::Scope context_scope(context);
    TryCatch try_catch(isolate_);

    Local<Function> transform = transform_sync_.Get(isolate_);
    Local<Object> options = Object::New(isolate_);
    if (options->Set(context,
                     ReadOnlyString("mode"),
                     ReadOnlyString("strip-only")).IsNothing() ||
        options->Set(context,
                     ReadOnlyString("filename"),
                     StringValue(filename)).IsNothing()) {
      last_error_ = "Failed to create amaro options";
      return 1;
    }

    Local<Value> argv[] = { StringValue(source), options };
    Local<Value> result;
    if (!transform->Call(context, context->Global(), 2, argv).ToLocal(&result)) {
      last_error_ = FormatException(isolate_, &try_catch);
      return 1;
    }

    if (!result->IsObject()) {
      last_error_ = "amaro transformSync returned a non-object result";
      return 1;
    }

    Local<Object> result_object = result.As<Object>();
    Local<Value> code_value;
    if (!result_object->Get(context, ReadOnlyString("code")).ToLocal(&code_value) ||
        !code_value->IsString()) {
      last_error_ = "amaro transformSync returned no code";
      return 1;
    }

    std::string stripped = ToUtf8(isolate_, code_value);
    output->assign(stripped.begin(), stripped.end());
    return 0;
  }

  std::string_view last_error() const {
    return last_error_;
  }

 private:
  bool ReadFile(std::string_view path, std::string* out) {
    std::string path_string(path);
    uv_fs_t req;
    uv_file file = uv_fs_open(nullptr, &req, path_string.c_str(), O_RDONLY, 0,
                              nullptr);
    if (file < 0) {
      uv_fs_req_cleanup(&req);
      return false;
    }
    uv_fs_req_cleanup(&req);

    int err = uv_fs_fstat(nullptr, &req, file, nullptr);
    if (err < 0) {
      uv_fs_req_cleanup(&req);
      CloseFile(file);
      return false;
    }
    if (req.statbuf.st_size < 0) {
      uv_fs_req_cleanup(&req);
      CloseFile(file);
      return false;
    }
    size_t size = static_cast<size_t>(req.statbuf.st_size);
    uv_fs_req_cleanup(&req);

    out->assign(size, '\0');
    if (out->empty()) {
      out->clear();
      return CloseFile(file) >= 0;
    }

    size_t offset = 0;
    while (offset < out->size()) {
      uv_buf_t buf = uv_buf_init(out->data() + offset, out->size() - offset);
      err = uv_fs_read(nullptr, &req, file, &buf, 1, offset, nullptr);
      uv_fs_req_cleanup(&req);
      if (err <= 0) {
        CloseFile(file);
        return false;
      }
      offset += static_cast<size_t>(err);
    }

    err = uv_fs_close(nullptr, &req, file, nullptr);
    uv_fs_req_cleanup(&req);
    if (err < 0) {
      return false;
    }

    return true;
  }

  int RunScript(std::string_view name, std::string_view source) {
    HandleScope handle_scope(isolate_);
    Local<Context> context = context_.Get(isolate_);
    Context::Scope context_scope(context);
    TryCatch try_catch(isolate_);

    Local<String> source_value;
    Local<String> name_value;
    if (!ReadUtf8String(isolate_, source).ToLocal(&source_value) ||
        !ReadUtf8String(isolate_, name).ToLocal(&name_value)) {
      last_error_ = "Failed to create V8 strings for " + std::string(name);
      return 1;
    }

    ScriptOrigin origin(name_value);
    ScriptCompiler::Source script_source(source_value, origin);
    Local<Script> script;
    if (!ScriptCompiler::Compile(context, &script_source).ToLocal(&script) ||
        script->Run(context).IsEmpty()) {
      last_error_ = FormatException(isolate_, &try_catch);
      return 1;
    }
    isolate_->PerformMicrotaskCheckpoint();
    return 0;
  }

  Local<String> ReadOnlyString(const char* value) {
    return String::NewFromUtf8(isolate_,
                               value,
                               v8::NewStringType::kInternalized)
        .ToLocalChecked();
  }

  Local<String> StringValue(std::string_view value) {
    return String::NewFromUtf8(isolate_,
                               value.data(),
                               v8::NewStringType::kNormal,
                               static_cast<int>(value.size())).ToLocalChecked();
  }

  std::unique_ptr<Platform> platform_;
  std::unique_ptr<ArrayBuffer::Allocator> allocator_;
  Isolate* isolate_ = nullptr;
  Global<Context> context_;
  Global<Function> transform_sync_;
  std::string last_error_;
  bool platform_initialized_ = false;
  bool v8_initialized_ = false;
};

TypeScriptTranspiler::TypeScriptTranspiler() = default;

TypeScriptTranspiler::~TypeScriptTranspiler() = default;

int TypeScriptTranspiler::Initialize(const char* argv0) {
  if (impl_ == nullptr) {
    impl_ = std::make_unique<Impl>();
  }
  return impl_->Initialize(argv0);
}

int TypeScriptTranspiler::Strip(std::string_view source,
                                std::string_view filename,
                                std::vector<char>* output) {
  if (impl_ == nullptr) {
    return 1;
  }
  return impl_->Strip(source, filename, output);
}

std::string_view TypeScriptTranspiler::LastError() const {
  if (impl_ == nullptr) {
    return {};
  }
  return impl_->last_error();
}

}  // namespace js2c
}  // namespace node
