#ifndef NODE_BUFFER_H_
#define NODE_BUFFER_H_

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <assert.h>

namespace node {

/* A buffer is a chunk of memory stored outside the V8 heap, mirrored by an
 * object in javascript. The object is not totally opaque, one can access
 * individual bytes with [] and slice it into substrings or sub-buffers
 * without copying memory.
 *
 * // return an ascii encoded string - no memory is copied
 * buffer.asciiSlice(0, 3)
 */


class Buffer : public ObjectWrap {
 public:
  ~Buffer();

  typedef void (*free_callback)(char *data, void *hint);

  // C++ API for constructing fast buffer
  static v8::Handle<v8::Object> New(v8::Handle<v8::String> string);

  static void Initialize(v8::Handle<v8::Object> target);
  static Buffer* New(size_t length); // public constructor
  static Buffer* New(char *data, size_t len); // public constructor
  static Buffer* New(char *data, size_t length,
                     free_callback callback, void *hint); // public constructor
  static bool HasInstance(v8::Handle<v8::Value> val);

  static inline char* Data(v8::Handle<v8::Object> obj) {
    return (char*)obj->GetIndexedPropertiesExternalArrayData();
  }

  static inline size_t Length(v8::Handle<v8::Object> obj) {
    return (size_t)obj->GetIndexedPropertiesExternalArrayDataLength();
  }

  private:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  static v8::Handle<v8::Value> New(const v8::Arguments &args);
  static v8::Handle<v8::Value> BinarySlice(const v8::Arguments &args);
  static v8::Handle<v8::Value> AsciiSlice(const v8::Arguments &args);
  static v8::Handle<v8::Value> Base64Slice(const v8::Arguments &args);
  static v8::Handle<v8::Value> Utf8Slice(const v8::Arguments &args);
  static v8::Handle<v8::Value> BinaryWrite(const v8::Arguments &args);
  static v8::Handle<v8::Value> Base64Write(const v8::Arguments &args);
  static v8::Handle<v8::Value> AsciiWrite(const v8::Arguments &args);
  static v8::Handle<v8::Value> Utf8Write(const v8::Arguments &args);
  static v8::Handle<v8::Value> ByteLength(const v8::Arguments &args);
  static v8::Handle<v8::Value> MakeFastBuffer(const v8::Arguments &args);
  static v8::Handle<v8::Value> Copy(const v8::Arguments &args);

  Buffer(v8::Handle<v8::Object> wrapper, size_t length);
  void Replace(char *data, size_t length, free_callback callback, void *hint);

  size_t length_;
  char* data_;
  free_callback callback_;
  void* callback_hint_;
};


}  // namespace node buffer

#endif  // NODE_BUFFER_H_
