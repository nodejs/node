#include "node.h"
#include "node_buffer.h"
#include "node_internals.h"

#include "async_wrap-inl.h"
#include "env-inl.h"
#include "util-inl.h"

#include "v8.h"

#include "brotli/encode.h"
#include "brotli/decode.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <iostream>

namespace node {

using v8::ArrayBuffer;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint32Array;
using v8::Value;

namespace {

enum node_brotli_mode {
  NONE,
  COMPRESS,
  DECOMPRESS
};

class BrotliCtx : public AsyncWrap {
 public:
  BrotliCtx(Environment* env, Local<Object> wrap, node_brotli_mode mode)
      : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_BROTLI),
        enc_state_(nullptr),
        dec_state_(nullptr),
        op_(BROTLI_OPERATION_PROCESS),
        encoder_mode_(0),
        quality_(0),
        lgwin_(0),
        lgblock_(0),
        disable_literal_context_modeling_(0),
        size_hint_(0),
        npostfix_(0),
        ndirect_(0),
        disable_ring_buffer_reallocation_(0),
        large_window_(0),
        err_(0),
        init_done_(false),
        mode_(mode),
        write_in_progress_(false),
        pending_close_(false),
        refs_(0),
        write_result_(nullptr) {
    MakeWeak<BrotliCtx>(this);
    Wrap(wrap, this);
  }

  ~BrotliCtx() override {
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
    CHECK_LE(mode_, DECOMPRESS);

    if (mode_ == COMPRESS) {
      BrotliEncoderDestroyInstance(enc_state_);
    } else if (mode_ == DECOMPRESS) {
      BrotliDecoderDestroyInstance(dec_state_);
    }

    mode_ = NONE;
  }


  static void Close(const FunctionCallbackInfo<Value>& args) {
    BrotliCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    ctx->Close();
  }


  // write(op, in, in_off, in_len, out, out_off, out_len)
  template <bool async>
  static void Write(const FunctionCallbackInfo<Value>& args) {
    CHECK_EQ(args.Length(), 7);

    BrotliCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    CHECK(ctx->init_done_ && "write before init");
    CHECK(ctx->mode_ != NONE && "already finalized");

    CHECK_EQ(false, ctx->write_in_progress_ && "write already in progress");
    CHECK_EQ(false, ctx->pending_close_ && "close is pending");
    ctx->write_in_progress_ = true;
    ctx->Ref();

    CHECK_EQ(false, args[0]->IsUndefined() && "must provide op value");

    unsigned int op = args[0]->Uint32Value();
    CHECK(op <= BROTLI_OPERATION_FINISH && "Invalid op value");

    ctx->op_ = static_cast<BrotliEncoderOperation>(op);

    uint8_t *in;
    uint8_t *out;
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
      in_buf = args[1]->ToObject(env->context()).ToLocalChecked();
      in_off = args[2]->Uint32Value();
      in_len = args[3]->Uint32Value();

      CHECK(Buffer::IsWithinBounds(in_off, in_len, Buffer::Length(in_buf)));
      in = reinterpret_cast<uint8_t *>(Buffer::Data(in_buf) + in_off);
    }

    CHECK(Buffer::HasInstance(args[4]));
    Local<Object> out_buf = args[4]->ToObject(env->context()).ToLocalChecked();
    out_off = args[5]->Uint32Value();
    out_len = args[6]->Uint32Value();
    CHECK(Buffer::IsWithinBounds(out_off, out_len, Buffer::Length(out_buf)));
    out = reinterpret_cast<uint8_t *>(Buffer::Data(out_buf) + out_off);

    // build up the work request
    uv_work_t* work_req = &(ctx->work_req_);

    ctx->avail_in_ = in_len;
    ctx->next_in_ = in;
    ctx->avail_out_ = out_len;
    ctx->next_out_ = out;

    if (!async) {
      // sync version
      env->PrintSyncTrace();
      Process(work_req);
      if (CheckError(ctx)) {
        ctx->write_result_[0] = ctx->avail_out_;
        ctx->write_result_[1] = ctx->avail_in_;
        ctx->write_in_progress_ = false;
        ctx->Unref();
      }
      return;
    }

    // async version
    uv_queue_work(env->event_loop(), work_req,
                  BrotliCtx::Process, BrotliCtx::After);
  }


  // thread pool!
  // This function may be called multiple times on the uv_work pool
  // for a single write() call, until all of the input bytes have
  // been consumed.
  static void Process(uv_work_t* work_req) {
    BrotliCtx *ctx = ContainerOf(&BrotliCtx::work_req_, work_req);

    const uint8_t **next_in =  const_cast<const uint8_t **>(&ctx->next_in_);

    // If the avail_out is left at 0, then it means that it ran out
    // of room.  If there was avail_out left over, then it means
    // that all of the input was consumed.
    switch (ctx->mode_) {
      case COMPRESS:
        ctx->err_ = BrotliEncoderCompressStream(ctx->enc_state_,
                                                ctx->op_,
                                                &ctx->avail_in_,
                                                next_in,
                                                &ctx->avail_out_,
                                                &ctx->next_out_,
                                                &ctx->total_out_);
        break;

      case DECOMPRESS:
        ctx->err_ = BrotliDecoderDecompressStream(ctx->dec_state_,
                                                  &ctx->avail_in_,
                                                  next_in,
                                                  &ctx->avail_out_,
                                                  &ctx->next_out_,
                                                  &ctx->total_out_);
        break;

      default:
        UNREACHABLE();
    }

    // pass any errors back to the main thread to deal with.

    // now After will emit the output, and
    // either schedule another call to Process,
    // or shift the queue and call Process.
  }


  static bool CheckError(BrotliCtx* ctx) {
    // Acceptable error states depend on the type of zlib stream.

    if (ctx->mode_ == COMPRESS) {
      if (ctx->err_ == BROTLI_FALSE) {
        BrotliCtx::Error(ctx, "Brotli error");
        return false;
      }

      return true;
    }

    if (ctx->err_ == BROTLI_DECODER_RESULT_ERROR) {
      ctx->err_ = BrotliDecoderGetErrorCode(ctx->dec_state_);
      BrotliCtx::Error(ctx, BrotliDecoderErrorString(
        static_cast<BrotliDecoderErrorCode>(ctx->err_)));
      return false;
    }

    return true;
  }


  // v8 land!
  static void After(uv_work_t* work_req, int status) {
    CHECK_EQ(status, 0);

    BrotliCtx* ctx = ContainerOf(&BrotliCtx::work_req_, work_req);
    Environment* env = ctx->env();

    HandleScope handle_scope(env->isolate());
    Context::Scope context_scope(env->context());

    if (!CheckError(ctx))
      return;

    ctx->write_result_[0] = ctx->avail_out_;
    ctx->write_result_[1] = ctx->avail_in_;
    ctx->write_in_progress_ = false;

    // call the write() cb
    Local<Function> cb = PersistentToLocal(env->isolate(),
                                           ctx->write_js_callback_);
    ctx->MakeCallback(cb, 0, nullptr);

    ctx->Unref();
    if (ctx->pending_close_)
      ctx->Close();
  }

  static void Error(BrotliCtx* ctx, const char* message) {
    Environment* env = ctx->env();

    // If you hit this assertion, you forgot to enter the v8::Context first.
    CHECK_EQ(env->context(), env->isolate()->GetCurrentContext());

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
    CHECK(args[0]->IsInt32());
    node_brotli_mode mode =
      static_cast<node_brotli_mode>(args[0]->Int32Value());
    new BrotliCtx(env, args.This(), mode);
  }

  static void SetOptions(BrotliCtx *ctx) {
    switch (ctx->mode_) {
      case COMPRESS:
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_MODE, ctx->encoder_mode_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_QUALITY, ctx->quality_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_LGWIN, ctx->lgwin_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_LGBLOCK, ctx->lgblock_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_DISABLE_LITERAL_CONTEXT_MODELING,
                                  ctx->disable_literal_context_modeling_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_SIZE_HINT, ctx->size_hint_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_NPOSTFIX, ctx->npostfix_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_NDIRECT, ctx->ndirect_);
        BrotliEncoderSetParameter(ctx->enc_state_,
                                  BROTLI_PARAM_LARGE_WINDOW,
                                  ctx->large_window_);
        break;

      case DECOMPRESS:
        BrotliDecoderSetParameter(ctx->dec_state_,
          BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION,
          ctx->disable_ring_buffer_reallocation_);
        BrotliDecoderSetParameter(ctx->dec_state_,
          BROTLI_DECODER_PARAM_LARGE_WINDOW,
          ctx->large_window_);
        break;

      default:
        UNREACHABLE();
    }
  }

  // just pull the ints out of the args and call the other Init
  static void Init(const FunctionCallbackInfo<Value>& args) {
    CHECK(args.Length() == 12 &&
      "init(mode, quality, lgwin, lgblock, disableLiteralContextModeling, "
      "sizeHint, npostfix, ndirect, disableRingBufferReallocation, "
      "largeWindow, writeResult, writeCallback)");

    BrotliCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());

    ctx->encoder_mode_ = args[0]->Uint32Value();
    ctx->quality_ = args[1]->Uint32Value();
    ctx->lgwin_ = args[2]->Uint32Value();
    ctx->lgblock_ = args[3]->Uint32Value();
    ctx->disable_literal_context_modeling_ = args[4]->Uint32Value();
    ctx->size_hint_ = args[5]->Uint32Value();
    ctx->npostfix_ = args[6]->Uint32Value();
    ctx->ndirect_ = args[7]->Uint32Value();
    ctx->disable_ring_buffer_reallocation_ = args[8]->Uint32Value();
    ctx->large_window_ = args[9]->Uint32Value();

    CHECK(args[10]->IsUint32Array());
    Local<Uint32Array> array = args[10].As<Uint32Array>();
    Local<ArrayBuffer> ab = array->Buffer();
    uint32_t* write_result = static_cast<uint32_t*>(ab->GetContents().Data());

    Local<Function> write_js_callback = args[11].As<Function>();

    switch (ctx->mode_) {
      case COMPRESS:
        ctx->enc_state_ = BrotliEncoderCreateInstance(0, 0, nullptr);
        break;
      case DECOMPRESS:
        ctx->dec_state_ = BrotliDecoderCreateInstance(0, 0, nullptr);
        break;
      default:
        UNREACHABLE();
    }

    SetOptions(ctx);

    ctx->write_in_progress_ = false;
    ctx->init_done_ = true;

    if (ctx->enc_state_ == nullptr && ctx->dec_state_ == nullptr) {
      ctx->mode_ = NONE;
      return args.GetReturnValue().Set(false);
    }

    ctx->write_result_ = write_result;
    ctx->write_js_callback_.Reset(ctx->env()->isolate(), write_js_callback);
    return args.GetReturnValue().Set(true);
  }

  static void Params(const FunctionCallbackInfo<Value>& args) {
    CHECK(args.Length() == 2 && "params(param, value)");
    BrotliCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    Params(ctx, args[0]->Int32Value(), args[1]->Int32Value());
  }

  static void Params(BrotliCtx* ctx, int param_int, int value) {
    ctx->err_ = BROTLI_TRUE;

    switch (ctx->mode_) {
      case COMPRESS: {
        BrotliEncoderParameter param =
          static_cast<BrotliEncoderParameter>(param_int);
        ctx->err_ = BrotliEncoderSetParameter(ctx->enc_state_, param, value);
        break;
      }

      case DECOMPRESS: {
        BrotliDecoderParameter param =
          static_cast<BrotliDecoderParameter>(param_int);
        ctx->err_ = BrotliDecoderSetParameter(ctx->dec_state_, param, value);
        break;
      }

      default:
        UNREACHABLE();
    }

    if (ctx->err_ != BROTLI_TRUE) {
      BrotliCtx::Error(ctx, "Failed to set parameters");
    }
  }

  static void Reset(const FunctionCallbackInfo<Value> &args) {
    BrotliCtx* ctx;
    ASSIGN_OR_RETURN_UNWRAP(&ctx, args.Holder());
    Reset(ctx);
  }

  static void Reset(BrotliCtx* ctx) {
    switch (ctx->mode_) {
      case COMPRESS:
        BrotliEncoderDestroyInstance(ctx->enc_state_);
        ctx->enc_state_ = BrotliEncoderCreateInstance(0, 0, nullptr);
        break;
      case DECOMPRESS:
        BrotliDecoderDestroyInstance(ctx->dec_state_);
        ctx->dec_state_ = BrotliDecoderCreateInstance(0, 0, nullptr);
        break;
      default:
        UNREACHABLE();
    }

    if (ctx->enc_state_ == nullptr && ctx->dec_state_ == nullptr) {
      BrotliCtx::Error(ctx, "Failed to reset stream");
    }

    SetOptions(ctx);
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
      MakeWeak<BrotliCtx>(this);
    }
  }

  BrotliEncoderState *enc_state_;
  BrotliDecoderState *dec_state_;

  size_t avail_in_;
  uint8_t *next_in_;
  size_t avail_out_;
  uint8_t *next_out_;
  size_t total_out_;

  BrotliEncoderOperation op_;
  uint32_t encoder_mode_;
  uint32_t quality_;
  uint32_t lgwin_;
  uint32_t lgblock_;
  uint32_t disable_literal_context_modeling_;
  uint32_t size_hint_;
  uint32_t npostfix_;
  uint32_t ndirect_;
  uint32_t disable_ring_buffer_reallocation_;
  uint32_t large_window_;

  int err_;
  bool init_done_;
  node_brotli_mode mode_;
  uv_work_t work_req_;
  bool write_in_progress_;
  bool pending_close_;
  unsigned int refs_;
  uint32_t* write_result_;
  Persistent<Function> write_js_callback_;
};


void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Local<FunctionTemplate> brotli = env->NewFunctionTemplate(BrotliCtx::New);

  brotli->InstanceTemplate()->SetInternalFieldCount(1);

  AsyncWrap::AddWrapMethods(env, brotli);
  env->SetProtoMethod(brotli, "write", BrotliCtx::Write<true>);
  env->SetProtoMethod(brotli, "writeSync", BrotliCtx::Write<false>);
  env->SetProtoMethod(brotli, "init", BrotliCtx::Init);
  env->SetProtoMethod(brotli, "close", BrotliCtx::Close);
  env->SetProtoMethod(brotli, "params", BrotliCtx::Params);
  env->SetProtoMethod(brotli, "reset", BrotliCtx::Reset);

  Local<String> brotliString = FIXED_ONE_BYTE_STRING(env->isolate(), "Brotli");
  brotli->SetClassName(brotliString);
  target->Set(brotliString, brotli->GetFunction());

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "BROTLI_VERSION"),
              Number::New(env->isolate(), BrotliEncoderVersion()));
}

}  // anonymous namespace
}  // namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(brotli, node::Initialize)
