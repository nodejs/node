#ifndef GOOGLE_PROTOBUF_STUBS_CALLBACK_H_
#define GOOGLE_PROTOBUF_STUBS_CALLBACK_H_

#include <type_traits>

#include <google/protobuf/stubs/macros.h>

#include <google/protobuf/port_def.inc>

// ===================================================================
// emulates google3/base/callback.h

namespace google {
namespace protobuf {

// Abstract interface for a callback.  When calling an RPC, you must provide
// a Closure to call when the procedure completes.  See the Service interface
// in service.h.
//
// To automatically construct a Closure which calls a particular function or
// method with a particular set of parameters, use the NewCallback() function.
// Example:
//   void FooDone(const FooResponse* response) {
//     ...
//   }
//
//   void CallFoo() {
//     ...
//     // When done, call FooDone() and pass it a pointer to the response.
//     Closure* callback = NewCallback(&FooDone, response);
//     // Make the call.
//     service->Foo(controller, request, response, callback);
//   }
//
// Example that calls a method:
//   class Handler {
//    public:
//     ...
//
//     void FooDone(const FooResponse* response) {
//       ...
//     }
//
//     void CallFoo() {
//       ...
//       // When done, call FooDone() and pass it a pointer to the response.
//       Closure* callback = NewCallback(this, &Handler::FooDone, response);
//       // Make the call.
//       service->Foo(controller, request, response, callback);
//     }
//   };
//
// Currently NewCallback() supports binding zero, one, or two arguments.
//
// Callbacks created with NewCallback() automatically delete themselves when
// executed.  They should be used when a callback is to be called exactly
// once (usually the case with RPC callbacks).  If a callback may be called
// a different number of times (including zero), create it with
// NewPermanentCallback() instead.  You are then responsible for deleting the
// callback (using the "delete" keyword as normal).
//
// Note that NewCallback() is a bit touchy regarding argument types.  Generally,
// the values you provide for the parameter bindings must exactly match the
// types accepted by the callback function.  For example:
//   void Foo(string s);
//   NewCallback(&Foo, "foo");          // WON'T WORK:  const char* != string
//   NewCallback(&Foo, string("foo"));  // WORKS
// Also note that the arguments cannot be references:
//   void Foo(const string& s);
//   string my_str;
//   NewCallback(&Foo, my_str);  // WON'T WORK:  Can't use referecnes.
// However, correctly-typed pointers will work just fine.
class PROTOBUF_EXPORT Closure {
 public:
  Closure() {}
  virtual ~Closure();

  virtual void Run() = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(Closure);
};

template<typename R>
class ResultCallback {
 public:
  ResultCallback() {}
  virtual ~ResultCallback() {}

  virtual R Run() = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ResultCallback);
};

template <typename R, typename A1>
class PROTOBUF_EXPORT ResultCallback1 {
 public:
  ResultCallback1() {}
  virtual ~ResultCallback1() {}

  virtual R Run(A1) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ResultCallback1);
};

template <typename R, typename A1, typename A2>
class PROTOBUF_EXPORT ResultCallback2 {
 public:
  ResultCallback2() {}
  virtual ~ResultCallback2() {}

  virtual R Run(A1,A2) = 0;

 private:
  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ResultCallback2);
};

namespace internal {

class PROTOBUF_EXPORT FunctionClosure0 : public Closure {
 public:
  typedef void (*FunctionType)();

  FunctionClosure0(FunctionType function, bool self_deleting)
    : function_(function), self_deleting_(self_deleting) {}
  ~FunctionClosure0();

  void Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    function_();
    if (needs_delete) delete this;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
};

template <typename Class>
class MethodClosure0 : public Closure {
 public:
  typedef void (Class::*MethodType)();

  MethodClosure0(Class* object, MethodType method, bool self_deleting)
    : object_(object), method_(method), self_deleting_(self_deleting) {}
  ~MethodClosure0() {}

  void Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    (object_->*method_)();
    if (needs_delete) delete this;
  }

 private:
  Class* object_;
  MethodType method_;
  bool self_deleting_;
};

template <typename Arg1>
class FunctionClosure1 : public Closure {
 public:
  typedef void (*FunctionType)(Arg1 arg1);

  FunctionClosure1(FunctionType function, bool self_deleting,
                   Arg1 arg1)
    : function_(function), self_deleting_(self_deleting),
      arg1_(arg1) {}
  ~FunctionClosure1() {}

  void Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    function_(arg1_);
    if (needs_delete) delete this;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
  Arg1 arg1_;
};

template <typename Class, typename Arg1>
class MethodClosure1 : public Closure {
 public:
  typedef void (Class::*MethodType)(Arg1 arg1);

  MethodClosure1(Class* object, MethodType method, bool self_deleting,
                 Arg1 arg1)
    : object_(object), method_(method), self_deleting_(self_deleting),
      arg1_(arg1) {}
  ~MethodClosure1() {}

  void Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    (object_->*method_)(arg1_);
    if (needs_delete) delete this;
  }

 private:
  Class* object_;
  MethodType method_;
  bool self_deleting_;
  Arg1 arg1_;
};

template <typename Arg1, typename Arg2>
class FunctionClosure2 : public Closure {
 public:
  typedef void (*FunctionType)(Arg1 arg1, Arg2 arg2);

  FunctionClosure2(FunctionType function, bool self_deleting,
                   Arg1 arg1, Arg2 arg2)
    : function_(function), self_deleting_(self_deleting),
      arg1_(arg1), arg2_(arg2) {}
  ~FunctionClosure2() {}

  void Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    function_(arg1_, arg2_);
    if (needs_delete) delete this;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
  Arg1 arg1_;
  Arg2 arg2_;
};

template <typename Class, typename Arg1, typename Arg2>
class MethodClosure2 : public Closure {
 public:
  typedef void (Class::*MethodType)(Arg1 arg1, Arg2 arg2);

  MethodClosure2(Class* object, MethodType method, bool self_deleting,
                 Arg1 arg1, Arg2 arg2)
    : object_(object), method_(method), self_deleting_(self_deleting),
      arg1_(arg1), arg2_(arg2) {}
  ~MethodClosure2() {}

  void Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    (object_->*method_)(arg1_, arg2_);
    if (needs_delete) delete this;
  }

 private:
  Class* object_;
  MethodType method_;
  bool self_deleting_;
  Arg1 arg1_;
  Arg2 arg2_;
};

template<typename R>
class FunctionResultCallback_0_0 : public ResultCallback<R> {
 public:
  typedef R (*FunctionType)();

  FunctionResultCallback_0_0(FunctionType function, bool self_deleting)
      : function_(function), self_deleting_(self_deleting) {}
  ~FunctionResultCallback_0_0() {}

  R Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    R result = function_();
    if (needs_delete) delete this;
    return result;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
};

template<typename R, typename P1>
class FunctionResultCallback_1_0 : public ResultCallback<R> {
 public:
  typedef R (*FunctionType)(P1);

  FunctionResultCallback_1_0(FunctionType function, bool self_deleting,
                             P1 p1)
      : function_(function), self_deleting_(self_deleting), p1_(p1) {}
  ~FunctionResultCallback_1_0() {}

  R Run() override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    R result = function_(p1_);
    if (needs_delete) delete this;
    return result;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
  P1 p1_;
};

template<typename R, typename Arg1>
class FunctionResultCallback_0_1 : public ResultCallback1<R, Arg1> {
 public:
  typedef R (*FunctionType)(Arg1 arg1);

  FunctionResultCallback_0_1(FunctionType function, bool self_deleting)
      : function_(function), self_deleting_(self_deleting) {}
  ~FunctionResultCallback_0_1() {}

  R Run(Arg1 a1) override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    R result = function_(a1);
    if (needs_delete) delete this;
    return result;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
};

template<typename R, typename P1, typename A1>
class FunctionResultCallback_1_1 : public ResultCallback1<R, A1> {
 public:
  typedef R (*FunctionType)(P1, A1);

  FunctionResultCallback_1_1(FunctionType function, bool self_deleting,
                             P1 p1)
      : function_(function), self_deleting_(self_deleting), p1_(p1) {}
  ~FunctionResultCallback_1_1() {}

  R Run(A1 a1) override {
    bool needs_delete = self_deleting_;  // read in case callback deletes
    R result = function_(p1_, a1);
    if (needs_delete) delete this;
    return result;
  }

 private:
  FunctionType function_;
  bool self_deleting_;
  P1 p1_;
};

template <typename T>
struct InternalConstRef {
  typedef typename std::remove_reference<T>::type base_type;
  typedef const base_type& type;
};

template<typename R, typename T>
class MethodResultCallback_0_0 : public ResultCallback<R> {
 public:
  typedef R (T::*MethodType)();
  MethodResultCallback_0_0(T* object, MethodType method, bool self_deleting)
      : object_(object),
        method_(method),
        self_deleting_(self_deleting) {}
  ~MethodResultCallback_0_0() {}

  R Run() {
    bool needs_delete = self_deleting_;
    R result = (object_->*method_)();
    if (needs_delete) delete this;
    return result;
  }

 private:
  T* object_;
  MethodType method_;
  bool self_deleting_;
};

template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename P5, typename P6, typename A1, typename A2>
class MethodResultCallback_6_2 : public ResultCallback2<R, A1, A2> {
 public:
  typedef R (T::*MethodType)(P1, P2, P3, P4, P5, P6, A1, A2);
  MethodResultCallback_6_2(T* object, MethodType method, bool self_deleting,
                           P1 p1, P2 p2, P3 p3, P4 p4, P5 p5, P6 p6)
      : object_(object),
        method_(method),
        self_deleting_(self_deleting),
        p1_(p1),
        p2_(p2),
        p3_(p3),
        p4_(p4),
        p5_(p5),
        p6_(p6) {}
  ~MethodResultCallback_6_2() {}

  R Run(A1 a1, A2 a2) override {
    bool needs_delete = self_deleting_;
    R result = (object_->*method_)(p1_, p2_, p3_, p4_, p5_, p6_, a1, a2);
    if (needs_delete) delete this;
    return result;
  }

 private:
  T* object_;
  MethodType method_;
  bool self_deleting_;
  typename std::remove_reference<P1>::type p1_;
  typename std::remove_reference<P2>::type p2_;
  typename std::remove_reference<P3>::type p3_;
  typename std::remove_reference<P4>::type p4_;
  typename std::remove_reference<P5>::type p5_;
  typename std::remove_reference<P6>::type p6_;
};

}  // namespace internal

// See Closure.
inline Closure* NewCallback(void (*function)()) {
  return new internal::FunctionClosure0(function, true);
}

// See Closure.
inline Closure* NewPermanentCallback(void (*function)()) {
  return new internal::FunctionClosure0(function, false);
}

// See Closure.
template <typename Class>
inline Closure* NewCallback(Class* object, void (Class::*method)()) {
  return new internal::MethodClosure0<Class>(object, method, true);
}

// See Closure.
template <typename Class>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)()) {
  return new internal::MethodClosure0<Class>(object, method, false);
}

// See Closure.
template <typename Arg1>
inline Closure* NewCallback(void (*function)(Arg1),
                            Arg1 arg1) {
  return new internal::FunctionClosure1<Arg1>(function, true, arg1);
}

// See Closure.
template <typename Arg1>
inline Closure* NewPermanentCallback(void (*function)(Arg1),
                                     Arg1 arg1) {
  return new internal::FunctionClosure1<Arg1>(function, false, arg1);
}

// See Closure.
template <typename Class, typename Arg1>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1),
                            Arg1 arg1) {
  return new internal::MethodClosure1<Class, Arg1>(object, method, true, arg1);
}

// See Closure.
template <typename Class, typename Arg1>
inline Closure* NewPermanentCallback(Class* object, void (Class::*method)(Arg1),
                                     Arg1 arg1) {
  return new internal::MethodClosure1<Class, Arg1>(object, method, false, arg1);
}

// See Closure.
template <typename Arg1, typename Arg2>
inline Closure* NewCallback(void (*function)(Arg1, Arg2),
                            Arg1 arg1, Arg2 arg2) {
  return new internal::FunctionClosure2<Arg1, Arg2>(
    function, true, arg1, arg2);
}

// See Closure.
template <typename Arg1, typename Arg2>
inline Closure* NewPermanentCallback(void (*function)(Arg1, Arg2),
                                     Arg1 arg1, Arg2 arg2) {
  return new internal::FunctionClosure2<Arg1, Arg2>(
    function, false, arg1, arg2);
}

// See Closure.
template <typename Class, typename Arg1, typename Arg2>
inline Closure* NewCallback(Class* object, void (Class::*method)(Arg1, Arg2),
                            Arg1 arg1, Arg2 arg2) {
  return new internal::MethodClosure2<Class, Arg1, Arg2>(
    object, method, true, arg1, arg2);
}

// See Closure.
template <typename Class, typename Arg1, typename Arg2>
inline Closure* NewPermanentCallback(
    Class* object, void (Class::*method)(Arg1, Arg2),
    Arg1 arg1, Arg2 arg2) {
  return new internal::MethodClosure2<Class, Arg1, Arg2>(
    object, method, false, arg1, arg2);
}

// See ResultCallback
template<typename R>
inline ResultCallback<R>* NewCallback(R (*function)()) {
  return new internal::FunctionResultCallback_0_0<R>(function, true);
}

// See ResultCallback
template<typename R>
inline ResultCallback<R>* NewPermanentCallback(R (*function)()) {
  return new internal::FunctionResultCallback_0_0<R>(function, false);
}

// See ResultCallback
template<typename R, typename P1>
inline ResultCallback<R>* NewCallback(R (*function)(P1), P1 p1) {
  return new internal::FunctionResultCallback_1_0<R, P1>(
      function, true, p1);
}

// See ResultCallback
template<typename R, typename P1>
inline ResultCallback<R>* NewPermanentCallback(
    R (*function)(P1), P1 p1) {
  return new internal::FunctionResultCallback_1_0<R, P1>(
      function, false, p1);
}

// See ResultCallback1
template<typename R, typename A1>
inline ResultCallback1<R, A1>* NewCallback(R (*function)(A1)) {
  return new internal::FunctionResultCallback_0_1<R, A1>(function, true);
}

// See ResultCallback1
template<typename R, typename A1>
inline ResultCallback1<R, A1>* NewPermanentCallback(R (*function)(A1)) {
  return new internal::FunctionResultCallback_0_1<R, A1>(function, false);
}

// See ResultCallback1
template<typename R, typename P1, typename A1>
inline ResultCallback1<R, A1>* NewCallback(R (*function)(P1, A1), P1 p1) {
  return new internal::FunctionResultCallback_1_1<R, P1, A1>(
      function, true, p1);
}

// See ResultCallback1
template<typename R, typename P1, typename A1>
inline ResultCallback1<R, A1>* NewPermanentCallback(
    R (*function)(P1, A1), P1 p1) {
  return new internal::FunctionResultCallback_1_1<R, P1, A1>(
      function, false, p1);
}

// See MethodResultCallback_0_0
template <typename R, typename T1, typename T2>
inline ResultCallback<R>* NewPermanentCallback(
    T1* object, R (T2::*function)()) {
  return new internal::MethodResultCallback_0_0<R, T1>(object, function, false);
}

// See MethodResultCallback_6_2
template <typename R, typename T, typename P1, typename P2, typename P3,
          typename P4, typename P5, typename P6, typename A1, typename A2>
inline ResultCallback2<R, A1, A2>* NewPermanentCallback(
    T* object, R (T::*function)(P1, P2, P3, P4, P5, P6, A1, A2),
    typename internal::InternalConstRef<P1>::type p1,
    typename internal::InternalConstRef<P2>::type p2,
    typename internal::InternalConstRef<P3>::type p3,
    typename internal::InternalConstRef<P4>::type p4,
    typename internal::InternalConstRef<P5>::type p5,
    typename internal::InternalConstRef<P6>::type p6) {
  return new internal::MethodResultCallback_6_2<R, T, P1, P2, P3, P4, P5, P6,
                                                A1, A2>(object, function, false,
                                                        p1, p2, p3, p4, p5, p6);
}

// A function which does nothing.  Useful for creating no-op callbacks, e.g.:
//   Closure* nothing = NewCallback(&DoNothing);
void PROTOBUF_EXPORT DoNothing();

}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_STUBS_CALLBACK_H_
