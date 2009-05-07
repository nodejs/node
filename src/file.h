#ifndef node_file_h
#define node_file_h

#include <v8.h>

namespace node {

class FileSystem {
public:
  static v8::Handle<v8::Value> Rename (const v8::Arguments& args);
  static int AfterRename (eio_req *req);

  static v8::Handle<v8::Value> Stat (const v8::Arguments& args);
  static int AfterStat (eio_req *req);

  static v8::Handle<v8::Value> StrError (const v8::Arguments& args);
};

class File : ObjectWrap {
public:
  static void Initialize (v8::Handle<v8::Object> target);

protected:
  File (v8::Handle<v8::Object> handle);
  ~File ();

  static v8::Handle<v8::Value> New (const v8::Arguments& args);

  static v8::Handle<v8::Value> Open (const v8::Arguments& args);
  static int AfterOpen (eio_req *req);

  static v8::Handle<v8::Value> Close (const v8::Arguments& args); 
  static int AfterClose (eio_req *req);

  static v8::Handle<v8::Value> Write (const v8::Arguments& args);
  static int AfterWrite (eio_req *req);

  static v8::Handle<v8::Value> Read (const v8::Arguments& args);
  static int AfterRead (eio_req *req);

  static v8::Handle<v8::Value> GetEncoding (v8::Local<v8::String> property, 
                                            const v8::AccessorInfo& info);
  static void SetEncoding (v8::Local<v8::String> property, 
                            v8::Local<v8::Value> value, 
                            const v8::AccessorInfo& info);

  bool HasUtf8Encoding (void);
  int GetFD (void);

  enum encoding encoding_;
};

} // namespace node
#endif // node_file_h
