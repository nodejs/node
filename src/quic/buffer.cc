#include "quic/buffer.h"  // NOLINT(build/include)

#include "quic/crypto.h"
#include "quic/session.h"
#include "quic/stream.h"
#include "quic/quic.h"
#include "aliased_struct-inl.h"
#include "async_wrap-inl.h"
#include "base_object-inl.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "node_bob-inl.h"
#include "node_sockaddr-inl.h"
#include "stream_base-inl.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::BackingStore;
using v8::EscapableHandleScope;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Promise;
using v8::Uint8Array;
using v8::Undefined;
using v8::Value;

namespace quic {

void Buffer::Source::set_owner(Stream* owner) {
  owner_ = owner;
  if (owner_ != nullptr)
    owner_->Resume();
}

void Buffer::Source::SetDonePromise() {
  BaseObjectPtr<BaseObject> ptr = GetStrongPtr();
  if (!ptr) return;

  Local<Promise::Resolver> resolver;
  if (!Promise::Resolver::New(env_->context()).ToLocal(&resolver))
    return;
  Local<Promise> promise = resolver.As<Promise>();
  ptr->object()->SetInternalField(Buffer::Source::kDonePromise, promise);
  USE(ptr->object()->Set(env_->context(), env_->done_string(), promise));
}

void Buffer::Source::ResolveDone() {
  BaseObjectPtr<BaseObject> ptr = GetStrongPtr();
  if (!ptr) return;

  HandleScope scope(env_->isolate());
  Local<Value> val = ptr->object()->GetInternalField(
      Buffer::Source::kDonePromise);

  if (val.IsEmpty() || val->IsUndefined())
    return;

  CHECK(val->IsPromise());
  Local<Promise> promise = val.As<Promise>();
  if (promise->State() == Promise::PromiseState::kPending) {
    Local<Promise::Resolver> resolver = promise.As<Promise::Resolver>();
    resolver->Resolve(env_->context(), Undefined(env_->isolate())).Check();
  }
}

void Buffer::Source::RejectDone(Local<Value> reason) {
  BaseObjectPtr<BaseObject> ptr = GetStrongPtr();
  if (!ptr) return;
  HandleScope scope(env_->isolate());
  Local<Value> val = ptr->object()->GetInternalField(
      Buffer::Source::kDonePromise);
  if (val.IsEmpty() || val->IsUndefined())
    return;

  CHECK(val->IsPromise());
  Local<Promise> promise = val.As<Promise>();
  if (promise->State() == Promise::PromiseState::kPending) {
    Local<Promise::Resolver> resolver = promise.As<Promise::Resolver>();
    resolver->Reject(env_->context(), reason).Check();
  }
}

Buffer::Chunk::Chunk(
    const std::shared_ptr<v8::BackingStore>& data,
    size_t length,
    size_t offset)
    : data_(std::move(data)),
      offset_(offset),
      length_(length),
      unacknowledged_(length) {}

std::unique_ptr<Buffer::Chunk> Buffer::Chunk::Create(
    Environment* env,
    const uint8_t* data,
    size_t len) {
  std::shared_ptr<v8::BackingStore> store =
      v8::ArrayBuffer::NewBackingStore(env->isolate(), len);
  memcpy(store->Data(), data, len);
  return std::unique_ptr<Buffer::Chunk>(
      new Buffer::Chunk(std::move(store), len));
}

std::unique_ptr<Buffer::Chunk> Buffer::Chunk::Create(
    const std::shared_ptr<v8::BackingStore>& data,
    size_t length,
    size_t offset) {
  return std::unique_ptr<Buffer::Chunk>(
      new Buffer::Chunk(std::move(data), length, offset));
}

MaybeLocal<Value> Buffer::Chunk::Release(Environment* env) {
  EscapableHandleScope scope(env->isolate());
  Local<Uint8Array> ret =
      Uint8Array::New(
          ArrayBuffer::New(env->isolate(), std::move(data_)),
          offset_,
          length_);
  CHECK(!data_);
  offset_ = 0;
  length_ = 0;
  read_ = 0;
  unacknowledged_ = 0;
  return scope.Escape(ret);
}

size_t Buffer::Chunk::Seek(size_t amount) {
  amount = std::min(amount, remaining());
  read_ += amount;
  CHECK_LE(read_, length_);
  return amount;
}

size_t Buffer::Chunk::Acknowledge(size_t amount) {
  amount = std::min(amount, unacknowledged_);
  unacknowledged_ -= amount;
  return amount;
}

ngtcp2_vec Buffer::Chunk::vec() const {
  uint8_t* ptr = static_cast<uint8_t*>(data_->Data());
  ptr += offset_ + read_;
  return ngtcp2_vec { ptr, remaining() };
}

void Buffer::Chunk::MemoryInfo(MemoryTracker* tracker) const {
  if (data_)
    tracker->TrackFieldWithSize("data", data_->ByteLength());
}

const uint8_t* Buffer::Chunk::data() const {
  uint8_t* ptr = static_cast<uint8_t*>(data_->Data());
  ptr += offset_ + read_;
  return ptr;
}

Buffer::Buffer(const BaseObjectPtr<Blob>& blob) {
  for (const auto& entry : blob->entries())
    Push(entry.store, entry.length, entry.offset);
  End();
}

Buffer::Buffer(
    const std::shared_ptr<BackingStore>& store,
    size_t length,
    size_t offset) {
  Push(store, length, offset);
  End();
}

void Buffer::Push(Environment* env, const uint8_t* data, size_t len) {
  CHECK(!ended_);
  queue_.emplace_back(Buffer::Chunk::Create(env, data, len));
  length_ += len;
  remaining_ += len;
}

void Buffer::Push(
    std::shared_ptr<v8::BackingStore> data,
    size_t length,
    size_t offset) {
  CHECK(!ended_);
  queue_.emplace_back(Buffer::Chunk::Create(std::move(data), length, offset));
  length_ += length;
  remaining_ += length;
}

size_t Buffer::Seek(size_t amount) {
  if (queue_.empty()) {
    CHECK_EQ(remaining_, 0);  // Remaining should be zero
    if (ended_)
      finished_ = true;
    return 0;
  }
  amount = std::min(amount, remaining_);
  size_t len = 0;
  while (amount > 0) {
    size_t chunk_remaining_ = queue_[head_]->remaining();
    size_t actual = queue_[head_]->Seek(amount);
    CHECK_LE(actual, amount);
    amount -= actual;
    remaining_ -= actual;
    len += actual;
    if (actual >= chunk_remaining_) {
      head_++;
      // head_ should never extend beyond queue size!
      CHECK_LE(head_, queue_.size());
    }
  }
  if (remaining_ == 0 && ended_)
    finished_ = true;
  return len;
}

size_t Buffer::Acknowledge(size_t amount) {
  if (queue_.empty())
    return 0;
  amount = std::min(amount, length_);
  size_t len = 0;
  while (amount > 0) {
    CHECK_GT(queue_.size(), 0);
    size_t actual = queue_.front()->Acknowledge(amount);

    CHECK_LE(actual, amount);
    amount -= actual;
    length_ -= actual;
    len += actual;
    // If we've acknowledged all of the bytes in the current
    // chunk, pop it to free the memory and decrement the
    // head_ pointer if necessary.
    if (queue_.front()->length() == 0) {
      queue_.pop_front();
      if (head_ > 0) head_--;
    }
  }
  return len;
}

void Buffer::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", queue_);
}

int Buffer::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  size_t len = 0;
  size_t numbytes = 0;
  int status = bob::Status::STATUS_CONTINUE;

  // There's no data to read.
  if (queue_.empty() || !remaining_) {
    status = ended_ ?
        bob::Status::STATUS_END :
        bob::Status::STATUS_BLOCK;
    std::move(next)(status, nullptr, 0, [](size_t len) {});
    return status;
  }

  // Ensure that there's storage space.
  MaybeStackBuffer<ngtcp2_vec, kMaxVectorCount> vec;
  size_t queue_size = queue_.size() - head_;

  max_count_hint = (max_count_hint == 0)
      ? queue_size
      : std::min(max_count_hint, queue_size);

  CHECK_IMPLIES(data == nullptr, count == 0);
  if (data == nullptr) {
    vec.AllocateSufficientStorage(max_count_hint);
    data = vec.out();
    count = max_count_hint;
  }

  // Build the list of buffers.
  for (size_t n = head_;
       n < queue_.size() && len < count;
       n++, len++) {
    data[len] = queue_[n]->vec();
    numbytes += data[len].len;
  }

  // If the buffer is ended, and the number of bytes
  // matches the total remaining, and OPTIONS_END is
  // used, set the status to STATUS_END.
  if (is_ended() &&
      numbytes == remaining() &&
      options & bob::OPTIONS_END) {
    status = bob::Status::STATUS_END;
  }

  // Pass the data back out to the caller.
  std::move(next)(
      status,
      data,
      len,
      [this](size_t len) {
        size_t actual = Seek(len);
        CHECK_LE(actual, len);
      });

  return status;
}

Maybe<size_t> Buffer::Release(Consumer* consumer) {
  if (queue_.empty())
    return Just(static_cast<size_t>(0));
  head_ = 0;
  length_ = 0;
  remaining_ = 0;
  return consumer->Process(std::move(queue_), ended_);
}

JSQuicBufferConsumer::JSQuicBufferConsumer(Environment* env, Local<Object> wrap)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_JSQUICBUFFERCONSUMER) {}

Maybe<size_t> JSQuicBufferConsumer::Process(
    Buffer::Chunk::Queue queue,
    bool ended) {
  EscapableHandleScope scope(env()->isolate());
  std::vector<Local<Value>> items;
  size_t len = 0;
  while (!queue.empty()) {
    Local<Value> val;
    len += queue.front()->length();
    // If this fails, the error is unrecoverable and neither
    // is the data. Return nothing to signal error and handle
    // upstream.
    if (!queue.front()->Release(env()).ToLocal(&val))
      return Nothing<size_t>();
    queue.pop_front();
    items.emplace_back(val);
  }

  Local<Value> args[] = {
    Array::New(env()->isolate(), items.data(), items.size()),
    ended ? v8::True(env()->isolate()) : v8::False(env()->isolate())
  };
  MakeCallback(env()->emit_string(), arraysize(args), args);
  return Just(len);
}

bool JSQuicBufferConsumer::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> JSQuicBufferConsumer::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl =
      state->jsquicbufferconsumer_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        JSQuicBufferConsumer::kInternalFieldCount);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "JSQuicBufferConsumer"));
    state->set_jsquicbufferconsumer_constructor_template(tmpl);
  }
  return tmpl;
}

void JSQuicBufferConsumer::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "JSQuicBufferConsumer",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

bool ArrayBufferViewSource::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> ArrayBufferViewSource::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl =
      state->arraybufferviewsource_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->Inherit(BaseObject::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Buffer::Source::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "ArrayBufferViewSource"));
    state->set_arraybufferviewsource_constructor_template(tmpl);
  }
  return tmpl;
}

void ArrayBufferViewSource::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "ArrayBufferViewSource",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

bool StreamSource::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> StreamSource::GetConstructorTemplate(Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state->streamsource_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Buffer::Source::kInternalFieldCount);
    env->SetProtoMethod(tmpl, "end", End);
    env->SetProtoMethod(tmpl, "write", Write);
    env->SetProtoMethod(tmpl, "writev", WriteV);
    tmpl->InstanceTemplate()->Set(env->owner_symbol(), Null(env->isolate()));
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "StreamSource"));
    state->set_streamsource_constructor_template(tmpl);
  }
  return tmpl;
}

void StreamSource::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "StreamSource",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

bool StreamBaseSource::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> StreamBaseSource::GetConstructorTemplate(
    Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state->streambasesource_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Buffer::Source::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "StreamBaseSource"));
    state->set_streambasesource_constructor_template(tmpl);
  }
  return tmpl;
}

void StreamBaseSource::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "StreamBaseSource",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

void JSQuicBufferConsumer::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new JSQuicBufferConsumer(env, args.This());
}

Local<FunctionTemplate> NullSource::GetConstructorTemplate(Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state->nullsource_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = FunctionTemplate::New(env->isolate());
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Buffer::Source::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "NullSource"));
    state->set_nullsource_constructor_template(tmpl);
  }
  return tmpl;
}

BaseObjectPtr<NullSource> NullSource::Create(Environment* env) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj)) {
    return BaseObjectPtr<NullSource>();
  }

  return MakeBaseObject<NullSource>(env, obj);
}

NullSource::NullSource(Environment* env, Local<Object> object)
    : Buffer::Source(env),
      BaseObject(env, object) {
  MakeWeak();
}

int NullSource::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  int status = bob::Status::STATUS_END;
  finished_ = true;
  std::move(next)(status, nullptr, 0, [](size_t len) {});
  return status;
}

void ArrayBufferViewSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsArrayBufferView());
  Environment* env = Environment::GetCurrent(args);
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  new ArrayBufferViewSource(
      env,
      args.This(),
      view->Buffer()->GetBackingStore(),
      view->ByteLength(),
      view->ByteOffset());
}

ArrayBufferViewSource::ArrayBufferViewSource(
    Environment* env,
    Local<Object> wrap,
    const std::shared_ptr<BackingStore>& store,
    size_t length,
    size_t offset)
    : Buffer::Source(env),
      BaseObject(env, wrap),
      buffer_(store, length, offset) {
  MakeWeak();
  SetDonePromise();
}

int ArrayBufferViewSource::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  return buffer_.Pull(std::move(next), options, data, count, max_count_hint);
}

size_t ArrayBufferViewSource::Acknowledge(
    uint64_t offset,
    size_t datalen) {
  return buffer_.Acknowledge(datalen);
}

size_t ArrayBufferViewSource::Seek(size_t amount) {
  size_t ret = buffer_.Seek(amount);
  if (buffer_.is_finished())
    ResolveDone();
  return ret;
}

void ArrayBufferViewSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("buffer", buffer_);
}

void StreamSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  new StreamSource(env, args.This());
}

StreamSource::StreamSource(Environment* env, Local<Object> wrap)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_STREAMSOURCE),
      Buffer::Source(env) {
  MakeWeak();
  SetDonePromise();
}

int StreamSource::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  return queue_.Pull(std::move(next), options, data, count, max_count_hint);
}

void StreamSource::set_closed() {
  queue_.End();
}

void StreamSource::End(const FunctionCallbackInfo<Value>& args) {
  StreamSource* source;
  ASSIGN_OR_RETURN_UNWRAP(&source, args.Holder());
  source->set_closed();
  if (source->owner())
    source->owner()->Resume();
}

void StreamSource::Write(const FunctionCallbackInfo<Value>& args) {
  StreamSource* source;
  ASSIGN_OR_RETURN_UNWRAP(&source, args.Holder());

  crypto::ArrayBufferOrViewContents<uint8_t> data(args[0]);
  source->queue_.Push(data.store(), data.size(), data.offset());

  if (source->owner())
    source->owner()->Resume();
}

void StreamSource::WriteV(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  StreamSource* source;
  ASSIGN_OR_RETURN_UNWRAP(&source, args.Holder());

  CHECK(args[0]->IsArray());
  Local<Array> data = args[0].As<Array>();
  for (size_t n = 0; n < data->Length(); n++) {
    Local<Value> item;
    if (!data->Get(env->context(), n).ToLocal(&item))
      return;
    crypto::ArrayBufferOrViewContents<uint8_t> data(item);
    source->queue_.Push(data.store(), data.size(), data.offset());
  }

  if (source->owner())
    source->owner()->Resume();
}

size_t StreamSource::Acknowledge(uint64_t offset, size_t datalen) {
  return queue_.Acknowledge(datalen);
}

size_t StreamSource::Seek(size_t amount) {
  size_t ret = queue_.Seek(amount);
  if (queue_.is_finished())
    ResolveDone();
  return ret;
}

void StreamSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", queue_);
}

void StreamBaseSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsObject());

  Environment* env = Environment::GetCurrent(args);

  StreamBase* wrap = StreamBase::FromObject(args[0].As<Object>());
  CHECK_NOT_NULL(wrap);
  StreamBaseSource* source =
      new StreamBaseSource(
          env,
          args.This(),
          wrap,
          BaseObjectPtr<AsyncWrap>(wrap->GetAsyncWrap()));
  wrap->PushStreamListener(source);
  wrap->ReadStart();
}

StreamBaseSource::StreamBaseSource(
    Environment* env,
    Local<Object> obj,
    StreamBase* resource,
    BaseObjectPtr<AsyncWrap> strong_ptr)
    : AsyncWrap(env, obj, AsyncWrap::PROVIDER_STREAMBASESOURCE),
      Buffer::Source(env),
      resource_(resource),
      strong_ptr_(std::move(strong_ptr)) {
  MakeWeak();
  SetDonePromise();
  CHECK_NOT_NULL(resource);
}

StreamBaseSource::~StreamBaseSource() {
  set_closed();
}

void StreamBaseSource::set_closed() {
  if (!buffer_.is_ended()) {
    buffer_.End();
    resource_->ReadStop();
    resource_->RemoveStreamListener(this);
  }
}

uv_buf_t StreamBaseSource::OnStreamAlloc(size_t suggested_size) {
  uv_buf_t buf;
  buf.base = Malloc<char>(suggested_size);
  buf.len = suggested_size;
  return buf;
}

void StreamBaseSource::OnStreamRead(ssize_t nread, const uv_buf_t& buf_) {
  if (nread == UV_EOF && buffer_.is_ended()) {
    CHECK_NULL(buf_.base);
    return;
  }
  CHECK(!buffer_.is_ended());
  CHECK_NOT_NULL(owner());

  if (nread < 0) {
    // TODO(@jasnell): Reject the done promise?
    set_closed();
    if (owner())
      owner()->Resume();
  } else if (nread > 0) {
    CHECK_NOT_NULL(buf_.base);
    size_t read = nread;
    std::shared_ptr<BackingStore> store =
        ArrayBuffer::NewBackingStore(
            static_cast<void*>(buf_.base),
            read,
            [](void* ptr, size_t len, void* deleter_data) {
              std::unique_ptr<char> delete_me(static_cast<char*>(ptr));
            },
            nullptr);
    buffer_.Push(std::move(store), store->ByteLength());
    if (owner())
      owner()->Resume();
  } else if (nread == 0 && buf_.base != nullptr) {
    std::unique_ptr<char> delete_me(buf_.base);
  }
}

int StreamBaseSource::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  return buffer_.Pull(std::move(next), options, data, count, max_count_hint);
}

size_t StreamBaseSource::Acknowledge(uint64_t offset, size_t datalen) {
  return buffer_.Acknowledge(datalen);
}

size_t StreamBaseSource::Seek(size_t amount) {
  size_t ret = buffer_.Seek(amount);
  if (buffer_.is_finished())
    ResolveDone();
  return ret;
}

void StreamBaseSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("queue", buffer_);
}

bool BlobSource::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> BlobSource::GetConstructorTemplate(Environment* env) {
  BindingState* state = BindingState::Get(env);
  Local<FunctionTemplate> tmpl = state->blobsource_constructor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(New);
    tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        Buffer::Source::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "BlobSource"));
    state->set_blobsource_constructor_template(tmpl);
  }
  return tmpl;
}

void BlobSource::Initialize(Environment* env, Local<Object> target) {
  env->SetConstructorFunction(
      target,
      "BlobSource",
      GetConstructorTemplate(env),
      Environment::SetConstructorFunctionFlag::NONE);
}

void BlobSource::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args);
  CHECK(Blob::HasInstance(env, args[0]));
  Blob* blob;
  ASSIGN_OR_RETURN_UNWRAP(&blob, args[0]);
  new BlobSource(env, args.This(), BaseObjectPtr<Blob>(blob));
}

BlobSource::BlobSource(
    Environment* env,
    Local<Object> wrap,
    BaseObjectPtr<Blob> blob)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_BLOBSOURCE),
      Buffer::Source(env),
      buffer_(blob) {
  MakeWeak();
  SetDonePromise();
}

int BlobSource::DoPull(
    bob::Next<ngtcp2_vec> next,
    int options,
    ngtcp2_vec* data,
    size_t count,
    size_t max_count_hint) {
  return buffer_.Pull(std::move(next), options, data, count, max_count_hint);
}

size_t BlobSource::Acknowledge(uint64_t offset, size_t datalen) {
  return buffer_.Acknowledge(datalen);
}

size_t BlobSource::Seek(size_t amount) {
  size_t ret = buffer_.Seek(amount);
  if (buffer_.is_finished())
    ResolveDone();
  return ret;
}

void BlobSource::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("buffer", buffer_);
}

}  // namespace quic
}  // namespace node
