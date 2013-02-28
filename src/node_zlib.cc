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


#include "v8.h"
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#include "zlib.h"
#include "node.h"
#include "node_buffer.h"


namespace node {
using namespace v8;


static Persistent<String> callback_sym;
static Persistent<String> onerror_sym;

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


void InitZlib(v8::Handle<v8::Object> target);


/**
 * Deflate/Inflate
 */
class ZCtx : public ObjectWrap {
 public:

  ZCtx(node_zlib_mode mode)
    : ObjectWrap()
    , init_done_(false)
    , level_(0)
    , windowBits_(0)
    , memLevel_(0)
    , strategy_(0)
    , err_(0)
    , dictionary_(NULL)
    , dictionary_len_(0)
    , flush_(0)
    , chunk_size_(0)
    , write_in_progress_(false)
    , mode_(mode)
  {
  }


  ~ZCtx() {
    Close();
  }


  void Close() {
    assert(!write_in_progress_ && "write in progress");
    assert(init_done_ && "close before init");
    assert(mode_ <= UNZIP);

    if (mode_ == DEFLATE || mode_ == GZIP || mode_ == DEFLATERAW) {
      (void)deflateEnd(&strm_);
      V8::AdjustAmountOfExternalAllocatedMemory(-kDeflateContextSize);
    } else if (mode_ == INFLATE || mode_ == GUNZIP || mode_ == INFLATERAW ||
               mode_ == UNZIP) {
      (void)inflateEnd(&strm_);
      V8::AdjustAmountOfExternalAllocatedMemory(-kInflateContextSize);
    }
    mode_ = NONE;

    if (dictionary_ != NULL) {
      delete[] dictionary_;
      dictionary_ = NULL;
    }
  }


  static Handle<Value> Close(const Arguments& args) {
    HandleScope scope;
    ZCtx *ctx = ObjectWrap::Unwrap<ZCtx>(args.This());
    ctx->Close();
    return scope.Close(Undefined());
  }


  // write(flush, in, in_off, in_len, out, out_off, out_len)
  static Handle<Value> Write(const Arguments& args) {
    HandleScope scope;
    assert(args.Length() == 7);

    ZCtx *ctx = ObjectWrap::Unwrap<ZCtx>(args.This());
    assert(ctx->init_done_ && "write before init");
    assert(ctx->mode_ != NONE && "already finalized");

    assert(!ctx->write_in_progress_ && "write already in progress");
    ctx->write_in_progress_ = true;
    ctx->Ref();

    assert(!args[0]->IsUndefined() && "must provide flush value");

    unsigned int flush = args[0]->Uint32Value();

    if (flush != Z_NO_FLUSH &&
        flush != Z_PARTIAL_FLUSH &&
        flush != Z_SYNC_FLUSH &&
        flush != Z_FULL_FLUSH &&
        flush != Z_FINISH &&
        flush != Z_BLOCK) {
      assert(0 && "Invalid flush value");
    }

    Bytef *in;
    Bytef *out;
    size_t in_off, in_len, out_off, out_len;

    if (args[1]->IsNull()) {
      // just a flush
      Bytef nada[1] = { 0 };
      in = nada;
      in_len = 0;
      in_off = 0;
    } else {
      assert(Buffer::HasInstance(args[1]));
      Local<Object> in_buf;
      in_buf = args[1]->ToObject();
      in_off = args[2]->Uint32Value();
      in_len = args[3]->Uint32Value();

      assert(in_off + in_len <= Buffer::Length(in_buf));
      in = reinterpret_cast<Bytef *>(Buffer::Data(in_buf) + in_off);
    }

    assert(Buffer::HasInstance(args[4]));
    Local<Object> out_buf = args[4]->ToObject();
    out_off = args[5]->Uint32Value();
    out_len = args[6]->Uint32Value();
    assert(out_off + out_len <= Buffer::Length(out_buf));
    out = reinterpret_cast<Bytef *>(Buffer::Data(out_buf) + out_off);

    // build up the work request
    uv_work_t* work_req = &(ctx->work_req_);

    ctx->strm_.avail_in = in_len;
    ctx->strm_.next_in = in;
    ctx->strm_.avail_out = out_len;
    ctx->strm_.next_out = out;
    ctx->flush_ = flush;

    // set this so that later on, I can easily tell how much was written.
    ctx->chunk_size_ = out_len;

    uv_queue_work(uv_default_loop(),
                  work_req,
                  ZCtx::Process,
                  ZCtx::After);

    return ctx->handle_;
  }


  // thread pool!
  // This function may be called multiple times on the uv_work pool
  // for a single write() call, until all of the input bytes have
  // been consumed.
  static void Process(uv_work_t* work_req) {
    ZCtx *ctx = container_of(work_req, ZCtx, work_req_);

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
      case INFLATE:
      case GUNZIP:
      case INFLATERAW:
        ctx->err_ = inflate(&ctx->strm_, ctx->flush_);

        // If data was encoded with dictionary
        if (ctx->err_ == Z_NEED_DICT && ctx->dictionary_ != NULL) {

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
        break;
      default:
        assert(0 && "wtf?");
    }

    // pass any errors back to the main thread to deal with.

    // now After will emit the output, and
    // either schedule another call to Process,
    // or shift the queue and call Process.
  }

  // v8 land!
  static void After(uv_work_t* work_req, int status) {
    assert(status == 0);

    HandleScope scope;
    ZCtx *ctx = container_of(work_req, ZCtx, work_req_);

    // Acceptable error states depend on the type of zlib stream.
    switch (ctx->err_) {
      case Z_OK:
      case Z_STREAM_END:
      case Z_BUF_ERROR:
        // normal statuses, not fatal
        break;
      case Z_NEED_DICT:
        if (ctx->dictionary_ == NULL) {
          ZCtx::Error(ctx, "Missing dictionary");
        } else {
          ZCtx::Error(ctx, "Bad dictionary");
        }
        return;
      default:
        // something else.
        ZCtx::Error(ctx, "Zlib error");
        return;
    }

    Local<Integer> avail_out = Integer::New(ctx->strm_.avail_out);
    Local<Integer> avail_in = Integer::New(ctx->strm_.avail_in);

    ctx->write_in_progress_ = false;

    // call the write() cb
    assert(ctx->handle_->Get(callback_sym)->IsFunction() &&
           "Invalid callback");
    Local<Value> args[2] = { avail_in, avail_out };
    MakeCallback(ctx->handle_, callback_sym, ARRAY_SIZE(args), args);

    ctx->Unref();
  }

  static void Error(ZCtx *ctx, const char *msg_) {
    const char *msg;
    if (ctx->strm_.msg != NULL) {
      msg = ctx->strm_.msg;
    } else {
      msg = msg_;
    }

    assert(ctx->handle_->Get(onerror_sym)->IsFunction() &&
           "Invalid error handler");
    HandleScope scope;
    Local<Value> args[2] = { String::New(msg),
                             Local<Value>::New(Number::New(ctx->err_)) };
    MakeCallback(ctx->handle_, onerror_sym, ARRAY_SIZE(args), args);

    // no hope of rescue.
    ctx->write_in_progress_ = false;
    ctx->Unref();
  }

  static Handle<Value> New(const Arguments& args) {
    HandleScope scope;
    if (args.Length() < 1 || !args[0]->IsInt32()) {
      return ThrowException(Exception::TypeError(String::New("Bad argument")));
    }
    node_zlib_mode mode = (node_zlib_mode) args[0]->Int32Value();

    if (mode < DEFLATE || mode > UNZIP) {
      return ThrowException(Exception::TypeError(String::New("Bad argument")));
    }

    ZCtx *ctx = new ZCtx(mode);
    ctx->Wrap(args.This());
    return args.This();
  }

  // just pull the ints out of the args and call the other Init
  static Handle<Value> Init(const Arguments& args) {
    HandleScope scope;

    assert((args.Length() == 4 || args.Length() == 5) &&
           "init(windowBits, level, memLevel, strategy, [dictionary])");

    ZCtx *ctx = ObjectWrap::Unwrap<ZCtx>(args.This());

    int windowBits = args[0]->Uint32Value();
    assert((windowBits >= 8 && windowBits <= 15) && "invalid windowBits");

    int level = args[1]->Uint32Value();
    assert((level >= -1 && level <= 9) && "invalid compression level");

    int memLevel = args[2]->Uint32Value();
    assert((memLevel >= 1 && memLevel <= 9) && "invalid memlevel");

    int strategy = args[3]->Uint32Value();
    assert((strategy == Z_FILTERED ||
            strategy == Z_HUFFMAN_ONLY ||
            strategy == Z_RLE ||
            strategy == Z_FIXED ||
            strategy == Z_DEFAULT_STRATEGY) && "invalid strategy");

    char* dictionary = NULL;
    size_t dictionary_len = 0;
    if (args.Length() >= 5 && Buffer::HasInstance(args[4])) {
      Local<Object> dictionary_ = args[4]->ToObject();

      dictionary_len = Buffer::Length(dictionary_);
      dictionary = new char[dictionary_len];

      memcpy(dictionary, Buffer::Data(dictionary_), dictionary_len);
    }

    Init(ctx, level, windowBits, memLevel, strategy,
         dictionary, dictionary_len);
    SetDictionary(ctx);
    return Undefined();
  }

  static Handle<Value> Reset(const Arguments &args) {
    HandleScope scope;

    ZCtx *ctx = ObjectWrap::Unwrap<ZCtx>(args.This());

    Reset(ctx);
    SetDictionary(ctx);
    return Undefined();
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
        V8::AdjustAmountOfExternalAllocatedMemory(kDeflateContextSize);
        break;
      case INFLATE:
      case GUNZIP:
      case INFLATERAW:
      case UNZIP:
        ctx->err_ = inflateInit2(&ctx->strm_, ctx->windowBits_);
        V8::AdjustAmountOfExternalAllocatedMemory(kInflateContextSize);
        break;
      default:
        assert(0 && "wtf?");
    }

    if (ctx->err_ != Z_OK) {
      ZCtx::Error(ctx, "Init error");
    }


    ctx->dictionary_ = reinterpret_cast<Bytef *>(dictionary);
    ctx->dictionary_len_ = dictionary_len;

    ctx->write_in_progress_ = false;
    ctx->init_done_ = true;
  }

  static void SetDictionary(ZCtx* ctx) {
    if (ctx->dictionary_ == NULL) return;

    ctx->err_ = Z_OK;

    switch (ctx->mode_) {
      case DEFLATE:
      case DEFLATERAW:
        ctx->err_ = deflateSetDictionary(&ctx->strm_,
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

  static void Reset(ZCtx* ctx) {
    ctx->err_ = Z_OK;

    switch (ctx->mode_) {
      case DEFLATE:
      case DEFLATERAW:
        ctx->err_ = deflateReset(&ctx->strm_);
        break;
      case INFLATE:
      case INFLATERAW:
        ctx->err_ = inflateReset(&ctx->strm_);
        break;
      default:
        break;
    }

    if (ctx->err_ != Z_OK) {
      ZCtx::Error(ctx, "Failed to reset stream");
    }
  }

 private:
  static const int kDeflateContextSize = 16384; // approximate
  static const int kInflateContextSize = 10240; // approximate

  bool init_done_;

  z_stream strm_;
  int level_;
  int windowBits_;
  int memLevel_;
  int strategy_;

  int err_;

  Bytef* dictionary_;
  size_t dictionary_len_;

  int flush_;

  int chunk_size_;

  bool write_in_progress_;

  uv_work_t work_req_;
  node_zlib_mode mode_;
};


void InitZlib(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> z = FunctionTemplate::New(ZCtx::New);

  z->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(z, "write", ZCtx::Write);
  NODE_SET_PROTOTYPE_METHOD(z, "init", ZCtx::Init);
  NODE_SET_PROTOTYPE_METHOD(z, "close", ZCtx::Close);
  NODE_SET_PROTOTYPE_METHOD(z, "reset", ZCtx::Reset);

  z->SetClassName(String::NewSymbol("Zlib"));
  target->Set(String::NewSymbol("Zlib"), z->GetFunction());

  callback_sym = NODE_PSYMBOL("callback");
  onerror_sym = NODE_PSYMBOL("onerror");

  // valid flush values.
  NODE_DEFINE_CONSTANT(target, Z_NO_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_PARTIAL_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_SYNC_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_FULL_FLUSH);
  NODE_DEFINE_CONSTANT(target, Z_FINISH);
  NODE_DEFINE_CONSTANT(target, Z_BLOCK);

  // return/error codes
  NODE_DEFINE_CONSTANT(target, Z_OK);
  NODE_DEFINE_CONSTANT(target, Z_STREAM_END);
  NODE_DEFINE_CONSTANT(target, Z_NEED_DICT);
  NODE_DEFINE_CONSTANT(target, Z_ERRNO);
  NODE_DEFINE_CONSTANT(target, Z_STREAM_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_DATA_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_MEM_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_BUF_ERROR);
  NODE_DEFINE_CONSTANT(target, Z_VERSION_ERROR);

  NODE_DEFINE_CONSTANT(target, Z_NO_COMPRESSION);
  NODE_DEFINE_CONSTANT(target, Z_BEST_SPEED);
  NODE_DEFINE_CONSTANT(target, Z_BEST_COMPRESSION);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_COMPRESSION);
  NODE_DEFINE_CONSTANT(target, Z_FILTERED);
  NODE_DEFINE_CONSTANT(target, Z_HUFFMAN_ONLY);
  NODE_DEFINE_CONSTANT(target, Z_RLE);
  NODE_DEFINE_CONSTANT(target, Z_FIXED);
  NODE_DEFINE_CONSTANT(target, Z_DEFAULT_STRATEGY);
  NODE_DEFINE_CONSTANT(target, ZLIB_VERNUM);

  NODE_DEFINE_CONSTANT(target, DEFLATE);
  NODE_DEFINE_CONSTANT(target, INFLATE);
  NODE_DEFINE_CONSTANT(target, GZIP);
  NODE_DEFINE_CONSTANT(target, GUNZIP);
  NODE_DEFINE_CONSTANT(target, DEFLATERAW);
  NODE_DEFINE_CONSTANT(target, INFLATERAW);
  NODE_DEFINE_CONSTANT(target, UNZIP);

  target->Set(String::NewSymbol("ZLIB_VERSION"), String::New(ZLIB_VERSION));
}

}  // namespace node

NODE_MODULE(node_zlib, node::InitZlib)
