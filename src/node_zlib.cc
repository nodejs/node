// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "node.h"
#include "node_buffer.h"

#include "async-wrap-inl.h"
#include "env-inl.h"
#include "util-inl.h"

#include "v8.h"
#include "zlib.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

namespace node {

using v8::Array;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace {

enum node_zlib_mode {
  NONE,
  DEFLATE,
  INFLATE,
  GZIP,
  GUNZIP,
  DEFLATERAW,
  INFLATERAW,
  UNZIP
};

#define GZIP_HEADER_ID1 0x1f
#define GZIP_HEADER_ID2 0x8b

/**
 * Deflate/Inflate
 */
class ZCtx : public AsyncWrap {
 public:
  ZCtx(Environment* env, Local<Object> wrap, node_zlib_mode mode)
      : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_ZLIB),
        dictionary_(nullptr),
        dictionary_len_(0),
        err_(0),
        flush_(0),
        init_done_(false),
        level_(0),
        memLevel_(0),
        mode_(mode),
        strategy_(0),
        windowBits_(0),
        write_in_progress_(false),
        pending_close_(false),
        refs_(0),
        gzip_id_bytes_read_(0) {
    MakeWeak<ZCtx>(this);
    Wrap(wrap, this);
  }


  ~ZCtx() override {
    CHECK_EQ(false, write_in_progress_ && "write in progress");
    Close();
  }

  void Close() {
    if (write_in_progress_) {
      pending_close_ = true;
      return;
    }

    pending_close_ = false;
    CHECK(init_done_ && "close before init");
    CHECK_LE(mode_, UNZIP);

    if (mode_ == DEFLATE || mode_ == GZIP || mode_ == DEFLATERAW) {
      (void)deflateEnd(&strm_);
      int64_t change_in_bytes = -static_cast<int64_t>(kDeflateContextSize);
      env()->isolate()->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);
    } else if (mode_ == INFLATE || mode_ == GUNZIP || mode_ == INFLATERAW ||
               mode_ == UNZIP) {
      (void)inflateEnd(&strm_);
      int64_t change_in_bytes = -static_cast<int64_t>(kInflateContextSize);
      env()->isolate()->AdjustAmountOfExternalAllocatedMemory(change_in_bytes);
    }
    mode_ = NONE;

    if (dictionary_ != nullptr) {
      delete[] dictionary_;
      dictionary_ = nullptr;
    }
  }


  static void Close(const FunctionCallbackInfo<Value>& args) {
    ZCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    ctx->Close();
  }


  // write(flush, in, in_off, in_len, out, out_off, out_len)
  template <bool async>
  static void Write(const FunctionCallbackInfo<Value>& args) {
    CHECK_EQ(args.Length(), 7);

    ZCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    CHECK(ctx->init_done_ && "write before init");
    CHECK(ctx->mode_ != NONE && "already finalized");

    CHECK_EQ(false, ctx->write_in_progress_ && "write already in progress");
    CHECK_EQ(false, ctx->pending_close_ && "close is pending");
    ctx->write_in_progress_ = true;
    ctx->Ref();

    CHECK_EQ(false, args[0]->IsUndefined() && "must provide flush value");

    unsigned int flush = args[0]->Uint32Value();

    if (flush != Z_NO_FLUSH &&
        flush != Z_PARTIAL_FLUSH &&
        flush != Z_SYNC_FLUSH &&
        flush != Z_FULL_FLUSH &&
        flush != Z_FINISH &&
        flush != Z_BLOCK) {
      CHECK(0 && "Invalid flush value");
    }

    Bytef *in;
    Bytef *out;
    size_t in_off, in_len, out_off, out_len;
    Environment* env = ctx->env();

    if (args[1]->IsNull()) {
      // just a flush
      in = nullptr;
      in_len = 0;
      in_off = 0;
    } else {
      CHECK(Buffer::HasInstance(args[1]));
      Local<Object> in_buf;
      in_buf = args[1]->ToObject(env->isolate());
      in_off = args[2]->Uint32Value();
      in_len = args[3]->Uint32Value();

      CHECK(Buffer::IsWithinBounds(in_off, in_len, Buffer::Length(in_buf)));
      in = reinterpret_cast<Bytef *>(Buffer::Data(in_buf) + in_off);
    }

    CHECK(Buffer::HasInstance(args[4]));
    Local<Object> out_buf = args[4]->ToObject(env->isolate());
    out_off = args[5]->Uint32Value();
    out_len = args[6]->Uint32Value();
    CHECK(Buffer::IsWithinBounds(out_off, out_len, Buffer::Length(out_buf)));
    out = reinterpret_cast<Bytef *>(Buffer::Data(out_buf) + out_off);

    // build up the work request
    uv_work_t* work_req = &(ctx->work_req_);

    ctx->strm_.avail_in = in_len;
    ctx->strm_.next_in = in;
    ctx->strm_.avail_out = out_len;
    ctx->strm_.next_out = out;
    ctx->flush_ = flush;

    if (!async) {
      // sync version
      ctx->env()->PrintSyncTrace();
      Process(work_req);
      if (CheckError(ctx))
        AfterSync(ctx, args);
      return;
    }

    // async version
    uv_queue_work(ctx->env()->event_loop(),
                  work_req,
                  ZCtx::Process,
                  ZCtx::After);

    args.GetReturnValue().Set(ctx->object());
  }


  static void AfterSync(ZCtx* ctx, const FunctionCallbackInfo<Value>& args) {
    Environment* env = ctx->env();
    Local<Integer> avail_out = Integer::New(env->isolate(),
                                            ctx->strm_.avail_out);
    Local<Integer> avail_in = Integer::New(env->isolate(),
                                           ctx->strm_.avail_in);

    ctx->write_in_progress_ = false;

    Local<Array> result = Array::New(env->isolate(), 2);
    result->Set(0, avail_in);
    result->Set(1, avail_out);
    args.GetReturnValue().Set(result);

    ctx->Unref();
  }


  // thread pool!
  // This function may be called multiple times on the uv_work pool
  // for a single write() call, until all of the input bytes have
  // been consumed.
  static void Process(uv_work_t* work_req) {
    ZCtx *ctx = ContainerOf(&ZCtx::work_req_, work_req);

    const Bytef* next_expected_header_byte = nullptr;

    // If the avail_out is left at 0, then it means that it ran out
    // of room.  If there was avail_out left over, then it means
    // that all of the input was consumed.
    switch (ctx->mode_) {
      case DEFLATE:
      case GZIP:
      case DEFLATERAW:
        ctx->err_ = deflate(&ctx->strm_, ctx->flush_);
        break;
      case UNZIP:
        if (ctx->strm_.avail_in > 0) {
          next_expected_header_byte = ctx->strm_.next_in;
        }

        switch (ctx->gzip_id_bytes_read_) {
          case 0:
            if (next_expected_header_byte == nullptr) {
              break;
            }

            if (*next_expected_header_byte == GZIP_HEADER_ID1) {
              ctx->gzip_id_bytes_read_ = 1;
              next_expected_header_byte++;

              if (ctx->strm_.avail_in == 1) {
                // The only available byte was already read.
                break;
              }
            } else {
              ctx->mode_ = INFLATE;
              break;
            }

            // fallthrough
          case 1:
            if (next_expected_header_byte == nullptr) {
              break;
            }

            if (*next_expected_header_byte == GZIP_HEADER_ID2) {
              ctx->gzip_id_bytes_read_ = 2;
              ctx->mode_ = GUNZIP;
            } else {
              // There is no actual difference between INFLATE and INFLATERAW
              // (after initialization).
              ctx->mode_ = INFLATE;
            }

            break;
          default:
            CHECK(0 && "invalid number of gzip magic number bytes read");
        }

        // fallthrough
      case INFLATE:
      case GUNZIP:
      case INFLATERAW:
        ctx->err_ = inflate(&ctx->strm_, ctx->flush_);

        // If data was encoded with dictionary (INFLATERAW will have it set in
        // SetDictionary, don't repeat that here)
        if (ctx->mode_ != INFLATERAW &&
            ctx->err_ == Z_NEED_DICT &&
            ctx->dictionary_ != nullptr) {
          // Load it
          ctx->err_ = inflateSetDictionary(&ctx->strm_,
                                           ctx->dictionary_,
                                           ctx->dictionary_len_);
          if (ctx->err_ == Z_OK) {
            // And try to decode again
            ctx->err_ = inflate(&ctx->strm_, ctx->flush_);
          } else if (ctx->err_ == Z_DATA_ERROR) {
            // Both inflateSetDictionary() and inflate() return Z_DATA_ERROR.
            // Make it possible for After() to tell a bad dictionary from bad
            // input.
            ctx->err_ = Z_NEED_DICT;
          }
        }

        while (ctx->strm_.avail_in > 0 &&
               ctx->mode_ == GUNZIP &&
               ctx->err_ == Z_STREAM_END &&
               ctx->strm_.next_in[0] != 0x00) {
          // Bytes remain in input buffer. Perhaps this is another compressed
          // member in the same archive, or just trailing garbage.
          // Trailing zero bytes are okay, though, since they are frequently
          // used for padding.

          Reset(ctx);
          ctx->err_ = inflate(&ctx->strm_, ctx->flush_);
        }
        break;
      default:
        CHECK(0 && "wtf?");
    }

    // pass any errors back to the main thread to deal with.

    // now After will emit the output, and
    // either schedule another call to Process,
    // or shift the queue and call Process.
  }


  static bool CheckError(ZCtx* ctx) {
    // Acceptable error states depend on the type of zlib stream.
    switch (ctx->err_) {
    case Z_OK:
    case Z_BUF_ERROR:
      if (ctx->strm_.avail_out != 0 && ctx->flush_ == Z_FINISH) {
        ZCtx::Error(ctx, "unexpected end of file");
        return false;
      }
    case Z_STREAM_END:
      // normal statuses, not fatal
      break;
    case Z_NEED_DICT:
      if (ctx->dictionary_ == nullptr)
        ZCtx::Error(ctx, "Missing dictionary");
      else
        ZCtx::Error(ctx, "Bad dictionary");
      return false;
    default:
      // something else.
      ZCtx::Error(ctx, "Zlib error");
      return false;
    }

    return true;
  }


  // v8 land!
  static void After(uv_work_t* work_req, int status) {
    CHECK_EQ(status, 0);

    ZCtx* ctx = ContainerOf(&ZCtx::work_req_, work_req);
    Environment* env = ctx->env();

    HandleScope handle_scope(env->isolate());
    Context::Scope context_scope(env->context());

    if (!CheckError(ctx))
      return;

    Local<Integer> avail_out = Integer::New(env->isolate(),
                                            ctx->strm_.avail_out);
    Local<Integer> avail_in = Integer::New(env->isolate(),
                                           ctx->strm_.avail_in);

    ctx->write_in_progress_ = false;

    // call the write() cb
    Local<Value> args[2] = { avail_in, avail_out };
    ctx->MakeCallback(env->callback_string(), arraysize(args), args);

    ctx->Unref();
    if (ctx->pending_close_)
      ctx->Close();
  }

  static void Error(ZCtx* ctx, const char* message) {
    Environment* env = ctx->env();

    // If you hit this assertion, you forgot to enter the v8::Context first.
    CHECK_EQ(env->context(), env->isolate()->GetCurrentContext());

    if (ctx->strm_.msg != nullptr) {
      message = ctx->strm_.msg;
    }

    HandleScope scope(env->isolate());
    Local<Value> args[2] = {
      OneByteString(env->isolate(), message),
      Number::New(env->isolate(), ctx->err_)
    };
    ctx->MakeCallback(env->onerror_string(), arraysize(args), args);

    // no hope of rescue.
    if (ctx->write_in_progress_)
      ctx->Unref();
    ctx->write_in_progress_ = false;
    if (ctx->pending_close_)
      ctx->Close();
  }

  static void New(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);

    if (args.Length() < 1 || !args[0]->IsInt32()) {
      return env->ThrowTypeError("Bad argument");
    }
    node_zlib_mode mode = static_cast<node_zlib_mode>(args[0]->Int32Value());

    if (mode < DEFLATE || mode > UNZIP) {
      return env->ThrowTypeError("Bad argument");
    }

    new ZCtx(env, args.This(), mode);
  }

  // just pull the ints out of the args and call the other Init
  static void Init(const FunctionCallbackInfo<Value>& args) {
    CHECK((args.Length() == 4 || args.Length() == 5) &&
           "init(windowBits, level, memLevel, strategy, [dictionary])");

    ZCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

    int windowBits = args[0]->Uint32Value();
    CHECK((windowBits >= 8 && windowBits <= 15) && "invalid windowBits");

    int level = args[1]->Int32Value();
    CHECK((level >= -1 && level <= 9) && "invalid compression level");

    int memLevel = args[2]->Uint32Value();
    CHECK((memLevel >= 1 && memLevel <= 9) && "invalid memlevel");

    int strategy = args[3]->Uint32Value();
    CHECK((strategy == Z_FILTERED ||
            strategy == Z_HUFFMAN_ONLY ||
            strategy == Z_RLE ||
            strategy == Z_FIXED ||
            strategy == Z_DEFAULT_STRATEGY) && "invalid strategy");

    char* dictionary = nullptr;
    size_t dictionary_len = 0;
    if (args.Length() >= 5 && Buffer::HasInstance(args[4])) {
      Local<Object> dictionary_ = args[4]->ToObject(args.GetIsolate());

      dictionary_len = Buffer::Length(dictionary_);
      dictionary = new char[dictionary_len];

      memcpy(dictionary, Buffer::Data(dictionary_), dictionary_len);
    }

    Init(ctx, level, windowBits, memLevel, strategy,
         dictionary, dictionary_len);
    SetDictionary(ctx);
  }

  static void Params(const FunctionCallbackInfo<Value>& args) {
    CHECK(args.Length() == 2 && "params(level, strategy)");
    ZCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    Params(ctx, args[0]->Int32Value(), args[1]->Int32Value());
  }

  static void Reset(const FunctionCallbackInfo<Value> &args) {
    ZCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    Reset(ctx);
    SetDictionary(ctx);
  }

  static void Init(ZCtx *ctx, int level, int windowBits, int memLevel,
                   int strategy, char* dictionary, size_t dictionary_len) {
    ctx->level_ = level;
    ctx->windowBits_ = windowBits;
    ctx->memLevel_ = memLevel;
    ctx->strategy_ = strategy;

    ctx->strm_.zalloc = Z_NULL;
    ctx->strm_.zfree = Z_NULL;
    ctx->strm_.opaque = Z_NULL;

    ctx->flush_ = Z_NO_FLUSH;

    ctx->err_ = Z_OK;

    if (ctx->mode_ == GZIP || ctx->mode_ == GUNZIP) {
      ctx->windowBits_ += 16;
    }

    if (ctx->mode_ == UNZIP) {
      ctx->windowBits_ += 32;
    }

    if (ctx->mode_ == DEFLATERAW || ctx->mode_ == INFLATERAW) {
      ctx->windowBits_ *= -1;
    }

    switch (ctx->mode_) {
      case DEFLATE:
      case GZIP:
      case DEFLATERAW:
        ctx->err_ = deflateInit2(&ctx->strm_,
                                 ctx->level_,
                                 Z_DEFLATED,
                                 ctx->windowBits_,
                                 ctx->memLevel_,
                                 ctx->strategy_);
        ctx->env()->isolate()
            ->AdjustAmountOfExternalAllocatedMemory(kDeflateContextSize);
        break;
      case INFLATE:
      case GUNZIP:
      case INFLATERAW:
      case UNZIP:
        ctx->err_ = inflateInit2(&ctx->strm_, ctx->windowBits_);
        ctx->env()->isolate()
            ->AdjustAmountOfExternalAllocatedMemory(kInflateContextSize);
        break;
      default:
        CHECK(0 && "wtf?");
    }

    ctx->dictionary_ = reinterpret_cast<Bytef *>(dictionary);
    ctx->dictionary_len_ = dictionary_len;

    ctx->write_in_progress_ = false;
    ctx->init_done_ = true;

    if (ctx->err_ != Z_OK) {
      if (dictionary != nullptr) {
        delete[] dictionary;
        ctx->dictionary_ = nullptr;
      }
      ctx->mode_ = NONE;
      ctx->env()->ThrowError("Init error");
    }
  }

  static void SetDictionary(ZCtx* ctx) {
    if (ctx->dictionary_ == nullptr)
      return;

    ctx->err_ = Z_OK;

    switch (ctx->mode_) {
      case DEFLATE:
      case DEFLATERAW:
        ctx->err_ = deflateSetDictionary(&ctx->strm_,
                                         ctx->dictionary_,
                                         ctx->dictionary_len_);
        break;
      case INFLATERAW:
        // The other inflate cases will have the dictionary set when inflate()
        // returns Z_NEED_DICT in Process()
        ctx->err_ = inflateSetDictionary(&ctx->strm_,
                                         ctx->dictionary_,
                                         ctx->dictionary_len_);
        break;
      default:
        break;
    }

    if (ctx->err_ != Z_OK) {
      ZCtx::Error(ctx, "Failed to set dictionary");
    }
  }

  static void Params(ZCtx* ctx, int level, int strategy) {
    ctx->err_ = Z_OK;

    switch (ctx->mode_) {
      case DEFLATE:
      case DEFLATERAW:
        ctx->err_ = deflateParams(&ctx->strm_, level, strategy);
        break;
      default:
        break;
    }

    if (ctx->err_ != Z_OK && ctx->err_ != Z_BUF_ERROR) {
      ZCtx::Error(ctx, "Failed to set parameters");
    }
  }

  static void Reset(ZCtx* ctx) {
    ctx->err_ = Z_OK;

    switch (ctx->mode_) {
      case DEFLATE:
      case DEFLATERAW:
      case GZIP:
        ctx->err_ = deflateReset(&ctx->strm_);
        break;
      case INFLATE:
      case INFLATERAW:
      case GUNZIP:
        ctx->err_ = inflateReset(&ctx->strm_);
        break;
      default:
        break;
    }

    if (ctx->err_ != Z_OK) {
      ZCtx::Error(ctx, "Failed to reset stream");
    }
  }

  size_t self_size() const override { return sizeof(*this); }

 private:
  void Ref() {
    if (++refs_ == 1) {
      ClearWeak();
    }
  }

  void Unref() {
    CHECK_GT(refs_, 0);
    if (--refs_ == 0) {
      MakeWeak<ZCtx>(this);
    }
  }

  static const int kDeflateContextSize = 16384;  // approximate
  static const int kInflateContextSize = 10240;  // approximate

  Bytef* dictionary_;
  size_t dictionary_len_;
  int err_;
  int flush_;
  bool init_done_;
  int level_;
  int memLevel_;
  node_zlib_mode mode_;
  int strategy_;
  z_stream strm_;
  int windowBits_;
  uv_work_t work_req_;
  bool write_in_progress_;
  bool pending_close_;
  unsigned int refs_;
  unsigned int gzip_id_bytes_read_;
};


void InitZlib(Local<Object> target,
              Local<Value> unused,
              Local<Context> context,
              void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Local<FunctionTemplate> z = env->NewFunctionTemplate(ZCtx::New);

  z->InstanceTemplate()->SetInternalFieldCount(1);

  AsyncWrap::AddWrapMethods(env, z);
  env->SetProtoMethod(z, "write", ZCtx::Write<true>);
  env->SetProtoMethod(z, "writeSync", ZCtx::Write<false>);
  env->SetProtoMethod(z, "init", ZCtx::Init);
  env->SetProtoMethod(z, "close", ZCtx::Close);
  env->SetProtoMethod(z, "params", ZCtx::Params);
  env->SetProtoMethod(z, "reset", ZCtx::Reset);

  Local<String> zlibString = FIXED_ONE_BYTE_STRING(env->isolate(), "Zlib");
  z->SetClassName(zlibString);
  target->Set(zlibString, z->GetFunction());

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "ZLIB_VERSION"),
              FIXED_ONE_BYTE_STRING(env->isolate(), ZLIB_VERSION));
}

}  // anonymous namespace
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(zlib, node::InitZlib)
