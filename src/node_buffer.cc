#include <node_buffer.h>

#include <assert.h>
#include <stdlib.h> // malloc, free
#include <v8.h>

#include <node.h>

namespace node {

using namespace v8;

#define SLICE_ARGS(start_arg, end_arg)                        \
  if (!start_arg->IsInt32() || !end_arg->IsInt32()) {           \
    return ThrowException(Exception::TypeError(               \
          String::New("Bad argument.")));                     \
  }                                                           \
  int32_t start = start_arg->Int32Value();                     \
  int32_t end = end_arg->Int32Value();                         \
  if (start < 0 || end < 0) {                                 \
    return ThrowException(Exception::TypeError(               \
          String::New("Bad argument.")));                     \
  }                                   

static Persistent<String> length_symbol;
static Persistent<FunctionTemplate> constructor_template;

bool IsBuffer(v8::Handle<v8::Value> val) {
  if (!val->IsObject()) return false;
  Local<Object> obj = val->ToObject();
  return constructor_template->HasInstance(obj);  
}


static inline struct buffer* buffer_root(buffer *buffer) {
  return buffer->root ? buffer->root : buffer;
}

/* Determines the absolute position for a relative offset */
static inline size_t buffer_abs_off(buffer *buffer, size_t off) {
  struct buffer *root = buffer_root(buffer);
  off += buffer->offset;
  return MIN(root->length, off);
}


static inline void buffer_ref(struct buffer *buffer) {
  assert(buffer->root == NULL);
  buffer->refs++;
}


static inline void buffer_unref(struct buffer *buffer) {
  assert(buffer->root == NULL);
  assert(buffer->refs > 0);
  buffer->refs--;
  if (buffer->refs == 0 && buffer->weak) free(buffer);
}


struct buffer* BufferUnwrap(v8::Handle<v8::Value> val) {
  assert(val->IsObject());
  HandleScope scope;
  Local<Object> obj = val->ToObject();
  assert(obj->InternalFieldCount() == 1);
  Local<External> ext = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<struct buffer*>(ext->Value());
}


static void RootWeakCallback(Persistent<Value> value, void *data)
{
  struct buffer *buffer = static_cast<struct buffer*>(data);
  assert(buffer->root == NULL); // this is the root
  assert(value == buffer->handle);
  buffer->handle.Dispose();
  if (buffer->refs) {
    buffer->weak = true;
  } else {
    free(buffer);
  }
}


static void SliceWeakCallback(Persistent<Value> value, void *data)
{
  struct buffer *buffer = static_cast<struct buffer*>(data);
  assert(buffer->root != NULL); // this is a slice
  assert(value == buffer->handle);
  buffer->handle.Dispose();
  buffer_unref(buffer->root);
}


static Handle<Value> Constructor(const Arguments &args) {
  HandleScope scope;

  size_t length;
  struct buffer *buffer;

  if (constructor_template->HasInstance(args[0])) {
    // slice slice
    SLICE_ARGS(args[1], args[2])

    struct buffer *parent = BufferUnwrap(args[0]);

    size_t start_abs = buffer_abs_off(parent, start);
    size_t end_abs   = buffer_abs_off(parent, end);
    assert(start_abs <= end_abs);
    length = end_abs - start_abs;

    void *d = malloc(sizeof(struct buffer));

    if (!d) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate enough memory")));

    }

    buffer = static_cast<struct buffer*>(d);

    buffer->length = length;
    buffer->offset = start_abs;
    buffer->weak = false;
    buffer->refs = 0;
    buffer->root = buffer_root(parent);
    buffer->handle = Persistent<Object>::New(args.This());
    buffer->handle.MakeWeak(buffer, SliceWeakCallback);

    buffer_ref(buffer->root);
  } else {
    // Root slice

    length = args[0]->Uint32Value();

    if (length < 1) {
      return ThrowException(Exception::TypeError(
            String::New("Bad argument. Length must be positive")));
    }

    // TODO alignment. modify the length?
    void *d = malloc(sizeof(struct buffer) + length - 1);

    if (!d) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate enough memory")));
    }

    buffer = static_cast<struct buffer*>(d);

    buffer->offset = 0;
    buffer->length = length;
    buffer->weak = false;
    buffer->refs = 0;
    buffer->root = NULL;
    buffer->handle = Persistent<Object>::New(args.This());
    buffer->handle.MakeWeak(buffer, RootWeakCallback);
  }

  args.This()->SetInternalField(0, v8::External::New(buffer));

  struct buffer *root = buffer_root(buffer);

  args.This()->
    SetIndexedPropertiesToExternalArrayData(&root->bytes + buffer->offset,
                                            kExternalUnsignedByteArray,
                                            length);

  args.This()->Set(length_symbol, Integer::New(length));

  return args.This();
}


class AsciiSliceExt: public String::ExternalAsciiStringResource {
 public:

  AsciiSliceExt(struct buffer *root, size_t start, size_t end) 
  {
    data_ = root->bytes + start;
    len_ = end - start;
    root_ = root;
    buffer_ref(root_);
  }

  ~AsciiSliceExt() {
    buffer_unref(root_);
  }

  const char* data() const {
    return data_;
  }

  size_t length() const {
    return len_;
  }

 private:
  const char *data_;
  size_t len_;
  struct buffer *root_;
};

static Handle<Value> AsciiSlice(const Arguments &args) {
  HandleScope scope;

  SLICE_ARGS(args[0], args[1])

  assert(args.This()->InternalFieldCount() == 1);
  struct buffer *parent = BufferUnwrap(args.This());

  size_t start_abs = buffer_abs_off(parent, start);
  size_t end_abs   = buffer_abs_off(parent, end);

  assert(start_abs <= end_abs);

  AsciiSliceExt *s = new AsciiSliceExt(buffer_root(parent), start_abs, end_abs);
  Local<String> string = String::NewExternal(s);

  struct buffer *root = buffer_root(parent);
  assert(root->refs > 0);
  
  return scope.Close(string);
}

static Handle<Value> Utf8Slice(const Arguments &args) {
  HandleScope scope;

  SLICE_ARGS(args[0], args[1])

  struct buffer *parent = BufferUnwrap(args.This());
  size_t start_abs = buffer_abs_off(parent, start);
  size_t end_abs   = buffer_abs_off(parent, end);
  assert(start_abs <= end_abs);

  struct buffer *root = buffer_root(parent);

  Local<String> string =
    String::New(reinterpret_cast<const char*>(&root->bytes + start_abs),
                end_abs - start_abs);
  return scope.Close(string);
}

static Handle<Value> Slice(const Arguments &args) {
  HandleScope scope;

  Local<Value> argv[3] = { args.This(), args[0], args[1] };

  Local<Object> slice =
    constructor_template->GetFunction()->NewInstance(3, argv);

  return scope.Close(slice);
}


// var charsWritten = buffer.utf8Write(string, offset, length);
static Handle<Value> Utf8Write(const Arguments &args) {
  HandleScope scope;

  struct buffer *buffer = BufferUnwrap(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  char *p = buffer_p(buffer, offset);
  if (buffer_p(buffer, offset) == NULL) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  size_t toWrite = args[2]->Int32Value();

  if (buffer_remaining(buffer, offset) < toWrite) {
    return ThrowException(Exception::TypeError(String::New(
            "Length is out of bounds")));
  }

  int written = s->WriteUtf8(p, toWrite);

  return scope.Close(Integer::New(written));
}


// var charsWritten = buffer.asciiWrite(string, offset, length);
static Handle<Value> AsciiWrite(const Arguments &args) {
  HandleScope scope;

  struct buffer *buffer = BufferUnwrap(args.This());

  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }

  Local<String> s = args[0]->ToString();

  size_t offset = args[1]->Int32Value();

  char *p = buffer_p(buffer, offset);
  if (buffer_p(buffer, offset) == NULL) {
    return ThrowException(Exception::TypeError(String::New(
            "Offset is out of bounds")));
  }

  size_t toWrite = args[2]->Int32Value();

  if (buffer_remaining(buffer, offset) < toWrite) {
    return ThrowException(Exception::TypeError(String::New(
            "Length is out of bounds")));
  }

  // TODO Expose the second argument of WriteAscii?
  // Could avoid doing slices when the string doesn't fit in a buffer.  V8
  // slice() does copy the string, so exposing that argument would help.

  int written = s->WriteAscii(p, 0, toWrite);

  return scope.Close(Integer::New(written));
}


static Handle<Value> Utf8Length(const Arguments &args) {
  HandleScope scope;
  if (!args[0]->IsString()) {
    return ThrowException(Exception::TypeError(String::New(
            "Argument must be a string")));
  }
  Local<String> s = args[0]->ToString();
  return scope.Close(Integer::New(s->Utf8Length()));
}


void InitBuffer(Handle<Object> target) {
  HandleScope scope;

  length_symbol = Persistent<String>::New(String::NewSymbol("length"));

  Local<FunctionTemplate> t = FunctionTemplate::New(Constructor);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Buffer"));

  // copy free
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "asciiSlice", AsciiSlice);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "slice", Slice);
  // TODO NODE_SET_PROTOTYPE_METHOD(t, "utf16Slice", Utf16Slice);
  // copy 
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "utf8Slice", Utf8Slice);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "utf8Write", Utf8Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "asciiWrite", AsciiWrite);

  NODE_SET_METHOD(constructor_template->GetFunction(), "utf8Length", Utf8Length);

  target->Set(String::NewSymbol("Buffer"), constructor_template->GetFunction());
}

}  // namespace node
