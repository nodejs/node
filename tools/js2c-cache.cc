#include "js2c-cache.h"
#include "v8.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace js2c_cache {

using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Script;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::V8;

static const char* wrap_lines[] = {
  "(function (exports, require, module, __filename, __dirname) { ",
  "\n});"
};


static char* ReadStdin(unsigned int* size, bool needs_wrap) {
  char* res = nullptr;
  unsigned int off = 0;

  if (needs_wrap) {
    int prefix_size = strlen(wrap_lines[0]);
    res = new char[prefix_size];
    memcpy(res + off, wrap_lines[0], prefix_size);
    off += prefix_size;
  }

  for (;;) {
    int nread;

    char tmp[16 * 1024];
    nread = fread(tmp, 1, sizeof(tmp), stdin);

    if (nread == 0) {
      if (!feof(stdin)) {
        off = 0;
      }
      break;
    }

    // This is very inefficient, but who cares?
    if (res == nullptr) {
      res = new char[nread];
    } else {
      char* bigger = new char[off + nread];
      memcpy(bigger, res, off);
      delete[] res;
      res = bigger;
    }

    memcpy(res + off, tmp, nread);
    off += nread;
  }

  if (off == 0) {
    delete[] res;
    return nullptr;
  }

  if (needs_wrap) {
    int postfix_size = strlen(wrap_lines[1]);

    char* bigger = new char[off + postfix_size];
    memcpy(bigger, res, off);
    delete[] res;
    res = bigger;

    memcpy(res + off, wrap_lines[1], postfix_size);
    off += postfix_size;
  }

  *size = off;
  return res;
}


static int CompileCache(Isolate* isolate,
                        Local<Context> context,
                        const char* input,
                        unsigned int input_size,
                        const char* filename) {
  Local<String> source_str =
      String::NewFromUtf8(isolate, input, String::kNormalString, input_size);

  ScriptOrigin origin(String::NewFromUtf8(isolate, filename));
  ScriptCompiler::Source source(source_str, origin);

  Local<Script> script = ScriptCompiler::Compile(
      context,
      &source,
      ScriptCompiler::kProduceCodeCache).ToLocalChecked();

  const ScriptCompiler::CachedData* data = source.GetCachedData();

  if (data->length == 0)
    return 0;

  fprintf(stdout, "0x%02x", data->data[0]);
  for (int i = 1; i < data->length; i++) {
    fprintf(stdout, ", 0x%02x", data->data[i]);
  }

  script.Clear();

  return 0;
}


static void FatalErrorHandler(const char* location, const char* message) {
  fprintf(stderr, "Fatal error at \"%s\":\n%s\n", location, message);
}


static int Start(const char* filename) {
  int err = 0;

  // Duplicate node's flags
  const char no_typed_array_heap[] = "--typed_array_max_size_in_heap=0";
  V8::SetFlagsFromString(no_typed_array_heap, sizeof(no_typed_array_heap) - 1);

  V8::Initialize();

  Isolate::CreateParams params;
  params.array_buffer_allocator = new ArrayBufferAllocator();
  Isolate* isolate = Isolate::New(params);

  isolate->SetFatalErrorHandler(FatalErrorHandler);

  {
    Isolate::Scope isolate_scope(isolate);
    HandleScope scope(isolate);

    Local<Context> context = Context::New(isolate);
    Context::Scope context_scope(context);

    bool needs_wrap = strcmp(filename, "src/node.js") != 0;

    unsigned int input_size;
    char* input = ReadStdin(&input_size, needs_wrap);
    if (input == nullptr) {
      fprintf(stderr, "Failed to read stdin, or empty input\n");
      err = -1;
    } else {
      err = CompileCache(isolate, context, input, input_size, filename);
    }

    delete[] input;
  }

  isolate->Dispose(),

  V8::Dispose();

  return err;
}

}  // namespace js2c_cache


int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Not enough arguments\n\n"
                    "Correct usage: js2c-cache file-name.js\n");
    return -1;
  }

  return js2c_cache::Start(argv[1]);
}
