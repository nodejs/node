#include <node_buffer.h>

#include <assert.h>
#include <stdlib.h> // malloc, free
#include <v8.h>

#include <string.h> // memcpy

#include <arpa/inet.h>  // htons, htonl

#include <node.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

namespace node {

using namespace v8;

#define SLICE_ARGS(start_arg, end_arg)                               \
  if (!start_arg->IsInt32() || !end_arg->IsInt32()) {                \
    return ThrowException(Exception::TypeError(                      \
          String::New("Bad argument.")));                            \
  }                                                                  \
  int32_t start = start_arg->Int32Value();                           \
  int32_t end = end_arg->Int32Value();                               \
  if (start < 0 || end < 0) {                                        \
    return ThrowException(Exception::TypeError(                      \
          String::New("Bad argument.")));                            \
  }                                                                  \
  if (!(start <= end)) {                                             \
    return ThrowException(Exception::Error(                          \
          String::New("Must have start <= end")));                   \
  }                                                                  \
  if ((size_t)end > parent->length_) {                               \
    return ThrowException(Exception::Error(                          \
          String::New("end cannot be longer than parent.length")));  \
  }


static Persistent<String> length_symbol;
static Persistent<String> chars_written_sym;
static Persistent<String> write_sym;
Persistent<FunctionTemplate> Buffer::constructor_template;


// Each javascript Buffer object is backed by a Blob object.
// the Blob is just a C-level chunk of bytes.
// It has a reference count.
struct Blob_ {
  unsigned int refs;
  size_t length;
  char *data;
};
typedef struct Blob_ Blob;


static inline Blob * blob_new(size_t length) {
  Blob * blob  = (Blob*) malloc(sizeof(Blob));
  if (!blob) return NULL;

  blob->data = (char*) malloc(length);
  if (!blob->data) {
    free(blob);
    return NULL;
  }

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Blob) + length);
  blob->length = length;
  blob->refs = 0;
  return blob;
}


static inline void blob_ref(Blob *blob) {
  blob->refs++;
}


static inline void blob_unref(Blob *blob) {
  assert(blob->refs > 0);
  if (--blob->refs == 0) {
    //fprintf(stderr, "free %d bytes\n", blob->length);
    V8::AdjustAmountOfExternalAllocatedMemory(-(sizeof(Blob) + blob->length));
    free(blob->data);
    free(blob);
  }
}

#if 0
// When someone calls buffer.asciiSlice, data is not copied. Instead V8
// references in the underlying Blob with this ExternalAsciiStringResource.
class AsciiSliceExt: public String::ExternalAsciiStringResource {
 friend class Buffer;
 public:
  AsciiSliceExt(Buffer *parent, size_t start, size_t end) {
    blob_ = parent->blob();
    blob_ref(blob_);

    assert(start <= end);
    length_ = end - start;
    assert(start + length_ <= parent->length());
    data_ = parent->data() + start;
  }


  ~AsciiSliceExt() {
    //fprintf(stderr, "free ascii slice (%d refs left)\n", blob_->refs);
    blob_unref(blob_);
  }


  const char* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const char *data_;
  size_t length_;
  Blob *blob_;
};
#endif

Buffer* Buffer::New(size_t size) {
  HandleScope scope;

  Local<Value> arg = Integer::NewFromUnsigned(size);
  Local<Object> b = constructor_template->GetFunction()->NewInstance(1, &arg);

  return ObjectWrap::Unwrap<Buffer>(b);
}


Handle<Value> Buffer::New(const Arguments &args) {
  HandleScope scope;

  Buffer *buffer;
  if (args[0]->IsInt32()) {
    // var buffer = new Buffer(1024);
    size_t length = args[0]->Uint32Value();
    buffer = new Buffer(length);

  } else if (args[0]->IsArray()) {
    Local<Array> a = Local<Array>::Cast(args[0]);
    buffer = new Buffer(a->Length());
    char *p = buffer->data();
    for (int i = 0; i < a->Length(); i++) {
      p[i] = a->Get(i)->Uint32Value();
    }
  } else if (args[0]->IsString()) {
    Local<String> s = args[0]->ToString();
    enum encoding e = ParseEncoding(args[1], UTF8);
    int length = e == UTF8 ? s->Utf8Length() : s->Length();
    buffer = new Buffer(length);
  } else if (Buffer::HasInstance(args[0]) && args.Length() > 2) {
    // var slice = new Buffer(buffer, 123, 130);
    // args: parent, start, end
    Buffer *parent = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());
    SLICE_ARGS(args[1], args[2])
    buffer = new Buffer(parent, start, end);
  } else {
    return ThrowException(Exception::TypeError(String::New("Bad argument")));
  }

  buffer->Wrap(args.This());
  args.This()->SetIndexedPropertiesToExternalArrayData(buffer->data(),
                                                       kExternalUnsignedByteArray,
                                                       buffer->length());
  args.This()->Set(length_symbol, Integer::New(buffer->length_));

  if (args[0]->IsString()) {
    if (write_sym.IsEmpty()) {
      write_sym = Persistent<String>::New(String::NewSymbol("write"));
    }

    Local<Value> write_v = args.This()->Get(write_sym);
    assert(write_v->IsFunction());
    Local<Function> write = Local<Function>::Cast(write_v);

    Local<Value> argv[2] = { args[0], args[1] };

    TryCatch try_catch;

    write->Call(args.This(), 2, argv);

    if (try_catch.HasCaught()) {
      FatalException(try_catch);
    }
  }

  return args.This();
}


Buffer::Buffer(size_t length) : ObjectWrap() {
  blob_ = blob_new(length);
  off_ = 0;
  length_ = length;

  blob_ref(blob_);

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Buffer));
}


Buffer::Buffer(Buffer *parent, size_t start, size_t end) : ObjectWrap() {
  blob_ = parent->blob_;
  assert(blob_->refs > 0);
  blob_ref(blob_);

  assert(start <= end);
  off_ = parent->off_ + start;
  length_ = end - start;
  assert(length_ <= parent->length_);

  V8::AdjustAmountOfExternalAllocatedMemory(sizeof(Buffer));
}


Buffer::~Buffer() {
  assert(blob_->refs > 0);
  //fprintf(stderr, "free buffer (%d refs left)\n", blob_->refs);
  blob_unref(blob_);
  V8::AdjustAmountOfExternalAllocatedMemory(-static_cast<long int>(sizeof(Buffer)));
}


char* Buffer::data() {
  return blob_->data + off_;
}


Handle<Value> Buffer::BinarySlice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

  const char *data = const_cast<char*>(parent->data() + start);
  //Local<String> string = String::New(data, end - start);

  Local<Value> b =  Encode(data, end - start, BINARY);

  return scope.Close(b);
}


Handle<Value> Buffer::AsciiSlice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])

#if 0
  AsciiSliceExt *ext = new AsciiSliceExt(parent, start, end);
  Local<String> string = String::NewExternal(ext);
  // There should be at least two references to the blob now - the parent
  // and the slice.
  assert(parent->blob_->refs >= 2);
#endif

  const char *data = const_cast<char*>(parent->data() + start);
  Local<String> string = String::New(data, end - start);


  return scope.Close(string);
}


Handle<Value> Buffer::Utf8Slice(const Arguments &args) {
  HandleScope scope;
  Buffer *parent = ObjectWrap::Unwrap<Buffer>(args.This());
  SLICE_ARGS(args[0], args[1])
  const char *data = const_cast<char*>(parent->data() + start);
  Local<String> string = String::New(data, end - start);
  return scope.Close(string);
}


Handle<Value> Buffer::Slice(const Arguments &args) {
  HandleScope scope;
  Local<Value> argv[3] = { args.This(), args[0], args[1] };
  Local<Object> slice =
    constructor_template->GetFunction()->NewInstance(3, argv);
  return scope.Close(slice);
}


// var bytesCopied = buffer.copy(target, targetStart, sourceStart, sourceEnd);
Handle<Value> Buffer::Copy(const Arguments &args) {
  HandleScope scope;

  Buffer *source = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(String::New(
            "First arg should be a Buffer")));
  }

  Buffer *target = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

  ssize_t target_start = args[1]->Int32Value();
  ssize_t source_start = args[2]->Int32Value();
  ssize_t source_end = args[3]->IsInt32() ? args[3]->Int32Value()
                                          : source->length();

  if (source_end < source_start) {
    return ThrowException(Exception::Error(String::New(
            "sourceEnd < sourceStart")));
  }

  // Copy 0 bytes; we're done
  if (source_end == source_start) {
    return scope.Close(Integer::New(0));
  }

  if (target_start < 0 || target_start >= target->length()) {
    return ThrowException(Exception::Error(String::New(
            "targetStart out of bounds")));
  }

  if (source_start < 0 || source_start >= source->length()) {
    return ThrowException(Exception::Error(String::New(
            "sourceStart out of bounds")));
  }

  if (source_end < 0 || source_end > source->length()) {
    return ThrowException(Exception::Error(String::New(
            "sourceEnd out of bounds")));
  }

  ssize_t to_copy = MIN(source_end - source_start,
                        target->length() - target_start);

  if (target->blob_ == source->blob_) {
    // need to use slightly slower memmove is the ranges might overlap
    memmove((void*)(target->data() + target_start),
            (const void*)(source->data() + source_start),
            to_copy);
  } else {
    memcpy((void*)(target->data() + target_start),
           (const void*)(source->data() + source_start),
           to_copy);
  }

  return scope.Close(Integer::New(to_copy));
}


// var charsWritten = buffer.utf8Write(string, offset);
Handle<Value> Buffer::Utf8Write(const Arguments &args) {
  HandleScope scope;
  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  if (s->Utf8Length() > 0 && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  const char *p = buffer->data() + offset;

  int char_written;

  int written = s->WriteUtf8((char*)p,
                             buffer->length_ - offset,
                             &char_written,
                             String::HINT_MANY_WRITES_EXPECTED);

  constructor_template->GetFunction()->Set(chars_written_sym,
                                           Integer::New(char_written));

  if (written > 0 && p[written-1] == '\0') written--;

  return scope.Close(Integer::New(written));
}


// var charsWritten = buffer.asciiWrite(string, offset);
Handle<Value> Buffer::AsciiWrite(const Arguments &args) {
  HandleScope scope;

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  if (s->Length() > 0 && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  const char *p = buffer->data() + offset;

  size_t towrite = MIN((unsigned long) s->Length(), buffer->length_ - offset);

  int written = s->WriteAscii((char*)p, 0, towrite, String::HINT_MANY_WRITES_EXPECTED);
  return scope.Close(Integer::New(written));
}


Handle<Value> Buffer::BinaryWrite(const Arguments &args) {
  HandleScope scope;

  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  if (s->Length() > 0 && offset >= buffer->length_) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  char *p = (char*)buffer->data() + offset;

  size_t towrite = MIN((unsigned long) s->Length(), buffer->length_ - offset);

  int written = DecodeWrite(p, towrite, s, BINARY);
  return scope.Close(Integer::New(written));
}


// buffer.unpack(format, index);
// Starting at 'index', unpacks binary from the buffer into an array.
// 'format' is a string
//
//  FORMAT  RETURNS
//    N     uint32_t   a 32bit unsigned integer in network byte order
//    n     uint16_t   a 16bit unsigned integer in network byte order
//    o     uint8_t    a 8bit unsigned integer
Handle<Value> Buffer::Unpack(const Arguments &args) {
  HandleScope scope;
  Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  String::AsciiValue format(args[0]->ToString());
  uint32_t index = args[1]->Uint32Value();

#define OUT_OF_BOUNDS ThrowException(Exception::Error(String::New("Out of bounds")))

  Local<Array> array = Array::New(format.length());

  uint8_t  uint8;
  uint16_t uint16;
  uint32_t uint32;

  for (int i = 0; i < format.length(); i++) {
    switch ((*format)[i]) {
      // 32bit unsigned integer in network byte order
      case 'N':
        if (index + 3 >= buffer->length_) return OUT_OF_BOUNDS;
        uint32 = htonl(*(uint32_t*)(buffer->data() + index));
        array->Set(Integer::New(i), Integer::NewFromUnsigned(uint32));
        index += 4;
        break;

      // 16bit unsigned integer in network byte order
      case 'n':
        if (index + 1 >= buffer->length_) return OUT_OF_BOUNDS;
        uint16 = htons(*(uint16_t*)(buffer->data() + index));
        array->Set(Integer::New(i), Integer::NewFromUnsigned(uint16));
        index += 2;
        break;

      // a single octet, unsigned.
      case 'o':
        if (index >= buffer->length_) return OUT_OF_BOUNDS;
        uint8 = (uint8_t)buffer->data()[index];
        array->Set(Integer::New(i), Integer::NewFromUnsigned(uint8));
        index += 1;
        break;

      default:
        return ThrowException(Exception::Error(
              String::New("Unknown format character")));
    }
  }

  return scope.Close(array);
}


// var nbytes = Buffer.byteLength("string", "utf8")
Handle<Value> Buffer::ByteLength(const Arguments &args) {
  HandleScope scope;

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();
  enum encoding e = ParseEncoding(args[1], UTF8);

  Local<Integer> length =
    Integer::New(e == UTF8 ? s->Utf8Length() : s->Length());

  return scope.Close(length);
}


void Buffer::Initialize(Handle<Object> target) {
  HandleScope scope;

  length_symbol = Persistent<String>::New(String::NewSymbol("length"));
  chars_written_sym = Persistent<String>::New(String::NewSymbol("_charsWritten"));

  Local<FunctionTemplate> t = FunctionTemplate::New(Buffer::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Buffer"));

  // copy free
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "binarySlice", Buffer::BinarySlice);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "asciiSlice", Buffer::AsciiSlice);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "slice", Buffer::Slice);
  // TODO NODE_SET_PROTOTYPE_METHOD(t, "utf16Slice", Utf16Slice);
  // copy
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "utf8Slice", Buffer::Utf8Slice);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "utf8Write", Buffer::Utf8Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "asciiWrite", Buffer::AsciiWrite);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "binaryWrite", Buffer::BinaryWrite);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "unpack", Buffer::Unpack);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "copy", Buffer::Copy);

  NODE_SET_METHOD(constructor_template->GetFunction(),
                  "byteLength",
                  Buffer::ByteLength);

  target->Set(String::NewSymbol("Buffer"), constructor_template->GetFunction());
}


}  // namespace node

NODE_MODULE(node_buffer, node::Buffer::Initialize);
