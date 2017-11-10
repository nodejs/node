## Nan::Callback

`Nan::Callback` makes it easier to use `v8::Function` handles as callbacks. A class that wraps a `v8::Function` handle, protecting it from garbage collection and making it particularly useful for storage and use across asynchronous execution.

 - <a href="#api_nan_callback"><b><code>Nan::Callback</code></b></a>

<a name="api_nan_callback"></a>
### Nan::Callback

```c++
class Callback {
 public:
  Callback();

  explicit Callback(const v8::Local<v8::Function> &fn);

  ~Callback();

  bool operator==(const Callback &other) const;

  bool operator!=(const Callback &other) const;

  v8::Local<v8::Function> operator*() const;

  v8::Local<v8::Value> operator()(v8::Local<v8::Object> target,
                                  int argc = 0,
                                  v8::Local<v8::Value> argv[] = 0) const;

  v8::Local<v8::Value> operator()(int argc = 0,
                                  v8::Local<v8::Value> argv[] = 0) const;

  void SetFunction(const v8::Local<v8::Function> &fn);

  v8::Local<v8::Function> GetFunction() const;

  bool IsEmpty() const;

  void Reset(const v8::Local<v8::Function> &fn);

  void Reset();

  v8::Local<v8::Value> Call(v8::Local<v8::Object> target,
                            int argc,
                            v8::Local<v8::Value> argv[]) const;

  v8::Local<v8::Value> Call(int argc, v8::Local<v8::Value> argv[]) const;
};
```

Example usage:

```c++
v8::Local<v8::Function> function;
Nan::Callback callback(function);
callback.Call(0, 0);
```
