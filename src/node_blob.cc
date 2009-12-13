#include <assert.h>
#include <stdlib.h> // malloc, free
#include <v8.h>
#include <node.h>

namespace node {

using namespace v8;

#define MIN(a,b) ((a) < (b) ? (a) : (b))

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

/* A blob is a chunk of memory stored outside the V8 heap mirrored by an
 * object in javascript. The object is not totally opaque, one can access
 * individual bytes with [] and one can slice the blob into substrings or
 * subblobs without copying memory.
 *
 * blob.asciiSlide(0, 3) // return an ascii encoded string - no memory iscopied
 * blob.slice(0, 3)      // returns another blob - no memory is copied
 *
 * Interally, each javascript blob object is backed by a "struct blob" object. 
 * These "struct blob" objects are either the root object (in the case that
 * blob->root == NULL) or slice objects (in which case blob->root != NULL).
 * The root blob is only GCed once all it's slices are GCed.
 */

struct blob {
  Persistent<Object> handle;  // both
  bool weak;                  // both
  struct blob *root;          // both (NULL for root)
  size_t offset;              // both (0 for root)
  size_t length;              // both
  unsigned int refs;          // root only
  char bytes[1];        // root only
};


static inline struct blob* blob_root(blob *blob) {
  return blob->root ? blob->root : blob;
}

/* Determines the absolute position for a relative offset */
static inline size_t blob_abs_off(blob *blob, size_t off) {
  struct blob *root = blob_root(blob);
  off += root->offset;
  return MIN(root->length, off);
}


static inline void blob_ref(struct blob *blob) {
  assert(blob->root == NULL);
  blob->refs++;
}


static inline void blob_unref(struct blob *blob) {
  assert(blob->root == NULL);
  assert(blob->refs > 0);
  blob->refs--;
  if (blob->refs == 0 && blob->weak) free(blob);
}


static inline struct blob* Unwrap(Handle<Value> val) {
  assert(val->IsObject());
  HandleScope scope;
  Local<Object> obj = val->ToObject();
  assert(obj->InternalFieldCount() == 1);
  Local<External> ext = Local<External>::Cast(obj->GetInternalField(0));
  return static_cast<struct blob*>(ext->Value());
}


static void RootWeakCallback(Persistent<Value> value, void *data)
{
  struct blob *blob = static_cast<struct blob*>(data);
  assert(blob->root == NULL); // this is the root
  assert(value == blob->handle);
  blob->handle.Dispose();
  if (blob->refs) {
    blob->weak = true;
  } else {
    free(blob);
  }
}


static void SliceWeakCallback(Persistent<Value> value, void *data)
{
  struct blob *blob = static_cast<struct blob*>(data);
  assert(blob->root != NULL); // this is a slice
  assert(value == blob->handle);
  blob->handle.Dispose();
  blob_unref(blob->root);
}


static Handle<Value> Constructor(const Arguments &args) {
  HandleScope scope;

  size_t length;
  struct blob *blob;

  if (constructor_template->HasInstance(args[0])) {
    // slice slice
    SLICE_ARGS(args[1], args[2])

    struct blob *parent = Unwrap(args[0]);

    size_t start_abs = blob_abs_off(parent, start);
    size_t end_abs   = blob_abs_off(parent, end);
    assert(start_abs <= end_abs);
    length = end_abs - start_abs;

    void *d = malloc(sizeof(struct blob));

    if (!d) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate enough memory")));

    }

    blob = static_cast<struct blob*>(d);

    blob->length = length;
    blob->offset = start_abs;
    blob->weak = false;
    blob->refs = 0;
    blob->root = blob_root(parent);
    blob->handle = Persistent<Object>::New(args.This());
    blob->handle.MakeWeak(blob, SliceWeakCallback);

    blob_ref(blob->root);
  } else {
    // Root slice

    length = args[0]->Uint32Value();

    if (length < 1) {
      return ThrowException(Exception::TypeError(
            String::New("Bad argument. Length must be positive")));
    }

    // TODO alignment. modify the length?
    void *d = malloc(sizeof(struct blob) + length - 1);

    if (!d) {
      V8::LowMemoryNotification();
      return ThrowException(Exception::Error(
            String::New("Could not allocate enough memory")));
    }

    blob = static_cast<struct blob*>(d);

    blob->offset = 0;
    blob->length = length;
    blob->weak = false;
    blob->refs = 0;
    blob->root = NULL;
    blob->handle = Persistent<Object>::New(args.This());
    blob->handle.MakeWeak(blob, RootWeakCallback);
  }

  args.This()->SetInternalField(0, v8::External::New(blob));

  struct blob *root = blob_root(blob);

  args.This()->
    SetIndexedPropertiesToExternalArrayData(&root->bytes + blob->offset,
                                            kExternalUnsignedByteArray,
                                            length);

  args.This()->Set(length_symbol, Integer::New(length));

  return args.This();
}


class AsciiSliceExt: public String::ExternalAsciiStringResource {
 public:

  AsciiSliceExt(struct blob *root, size_t start, size_t end) 
  {
    data_ = root->bytes + start;
    len_ = end - start;
    root_ = root;
    blob_ref(root_);
  }

  ~AsciiSliceExt() {
    blob_unref(root_);
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
  struct blob *root_;
};

static Handle<Value> AsciiSlice(const Arguments &args) {
  HandleScope scope;

  SLICE_ARGS(args[0], args[1])

  assert(args.This()->InternalFieldCount() == 1);
  struct blob *parent = Unwrap(args.This());

  size_t start_abs = blob_abs_off(parent, start);
  size_t end_abs   = blob_abs_off(parent, end);

  assert(start_abs <= end_abs);

  AsciiSliceExt *s = new AsciiSliceExt(blob_root(parent), start_abs, end_abs);
  Local<String> string = String::NewExternal(s);

  struct blob *root = blob_root(parent);
  assert(root->refs > 0);
  
  return scope.Close(string);
}

static Handle<Value> Utf8Slice(const Arguments &args) {
  HandleScope scope;

  SLICE_ARGS(args[0], args[1])

  struct blob *parent = Unwrap(args.This());
  size_t start_abs = blob_abs_off(parent, start);
  size_t end_abs   = blob_abs_off(parent, end);
  assert(start_abs <= end_abs);

  struct blob *root = blob_root(parent);

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

void InitBlob(Handle<Object> target) {
  HandleScope scope;

  length_symbol = Persistent<String>::New(String::NewSymbol("length"));

  Local<FunctionTemplate> t = FunctionTemplate::New(Constructor);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Blob"));

  // copy free
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "asciiSlice", AsciiSlice);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "slice", Slice);
  // TODO NODE_SET_PROTOTYPE_METHOD(t, "utf16Slice", Utf16Slice);
  // copy 
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "utf8Slice", Utf8Slice);

  target->Set(String::NewSymbol("Blob"), constructor_template->GetFunction());
}

}  // namespace node
