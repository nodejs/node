/*********************************************************************
 * NAN - Native Abstractions for Node.js
 *
 * Copyright (c) 2016 NAN contributors
 *
 * MIT License <https://github.com/nodejs/nan/blob/master/LICENSE.md>
 ********************************************************************/

#ifndef NAN_PERSISTENT_PRE_12_INL_H_
#define NAN_PERSISTENT_PRE_12_INL_H_

template<typename T>
class PersistentBase {
  v8::Persistent<T> persistent;
  template<typename U>
  friend v8::Local<U> New(const PersistentBase<U> &p);
  template<typename U, typename M>
  friend v8::Local<U> New(const Persistent<U, M> &p);
  template<typename U>
  friend v8::Local<U> New(const Global<U> &p);
  template<typename S> friend class ReturnValue;

 public:
  NAN_INLINE PersistentBase() :
      persistent() {}

  NAN_INLINE void Reset() {
    persistent.Dispose();
    persistent.Clear();
  }

  template<typename S>
  NAN_INLINE void Reset(const v8::Local<S> &other) {
    TYPE_CHECK(T, S);

    if (!persistent.IsEmpty()) {
      persistent.Dispose();
    }

    if (other.IsEmpty()) {
      persistent.Clear();
    } else {
      persistent = v8::Persistent<T>::New(other);
    }
  }

  template<typename S>
  NAN_INLINE void Reset(const PersistentBase<S> &other) {
    TYPE_CHECK(T, S);

    if (!persistent.IsEmpty()) {
      persistent.Dispose();
    }

    if (other.IsEmpty()) {
      persistent.Clear();
    } else {
      persistent = v8::Persistent<T>::New(other.persistent);
    }
  }

  NAN_INLINE bool IsEmpty() const { return persistent.IsEmpty(); }

  NAN_INLINE void Empty() { persistent.Clear(); }

  template<typename S>
  NAN_INLINE bool operator==(const PersistentBase<S> &that) {
    return this->persistent == that.persistent;
  }

  template<typename S>
  NAN_INLINE bool operator==(const v8::Local<S> &that) {
    return this->persistent == that;
  }

  template<typename S>
  NAN_INLINE bool operator!=(const PersistentBase<S> &that) {
    return !operator==(that);
  }

  template<typename S>
  NAN_INLINE bool operator!=(const v8::Local<S> &that) {
    return !operator==(that);
  }

  template<typename P>
  NAN_INLINE void SetWeak(
    P *parameter
    , typename WeakCallbackInfo<P>::Callback callback
    , WeakCallbackType type);

  NAN_INLINE void ClearWeak() { persistent.ClearWeak(); }

  NAN_INLINE void MarkIndependent() { persistent.MarkIndependent(); }

  NAN_INLINE bool IsIndependent() const { return persistent.IsIndependent(); }

  NAN_INLINE bool IsNearDeath() const { return persistent.IsNearDeath(); }

  NAN_INLINE bool IsWeak() const { return persistent.IsWeak(); }

 private:
  NAN_INLINE explicit PersistentBase(v8::Persistent<T> that) :
      persistent(that) { }
  NAN_INLINE explicit PersistentBase(T *val) : persistent(val) {}
  template<typename S, typename M> friend class Persistent;
  template<typename S> friend class Global;
  friend class ObjectWrap;
};

template<typename T>
class NonCopyablePersistentTraits {
 public:
  typedef Persistent<T, NonCopyablePersistentTraits<T> >
      NonCopyablePersistent;
  static const bool kResetInDestructor = false;
  template<typename S, typename M>
  NAN_INLINE static void Copy(const Persistent<S, M> &source,
                             NonCopyablePersistent *dest) {
    Uncompilable<v8::Object>();
  }

  template<typename O> NAN_INLINE static void Uncompilable() {
    TYPE_CHECK(O, v8::Primitive);
  }
};

template<typename T>
struct CopyablePersistentTraits {
  typedef Persistent<T, CopyablePersistentTraits<T> > CopyablePersistent;
  static const bool kResetInDestructor = true;
  template<typename S, typename M>
  static NAN_INLINE void Copy(const Persistent<S, M> &source,
                             CopyablePersistent *dest) {}
};

template<typename T, typename M> class Persistent :
    public PersistentBase<T> {
 public:
  NAN_INLINE Persistent() {}

  template<typename S> NAN_INLINE Persistent(v8::Handle<S> that)
      : PersistentBase<T>(v8::Persistent<T>::New(that)) {
    TYPE_CHECK(T, S);
  }

  NAN_INLINE Persistent(const Persistent &that) : PersistentBase<T>() {
    Copy(that);
  }

  template<typename S, typename M2>
  NAN_INLINE Persistent(const Persistent<S, M2> &that) :
      PersistentBase<T>() {
    Copy(that);
  }

  NAN_INLINE Persistent &operator=(const Persistent &that) {
    Copy(that);
    return *this;
  }

  template <class S, class M2>
  NAN_INLINE Persistent &operator=(const Persistent<S, M2> &that) {
    Copy(that);
    return *this;
  }

  NAN_INLINE ~Persistent() {
    if (M::kResetInDestructor) this->Reset();
  }

 private:
  NAN_INLINE T *operator*() const { return *PersistentBase<T>::persistent; }

  template<typename S, typename M2>
  NAN_INLINE void Copy(const Persistent<S, M2> &that) {
    TYPE_CHECK(T, S);

    this->Reset();

    if (!that.IsEmpty()) {
      this->persistent = v8::Persistent<T>::New(that.persistent);
      M::Copy(that, this);
    }
  }
};

template<typename T>
class Global : public PersistentBase<T> {
  struct RValue {
    NAN_INLINE explicit RValue(Global* obj) : object(obj) {}
    Global* object;
  };

 public:
  NAN_INLINE Global() : PersistentBase<T>(0) { }

  template <typename S>
  NAN_INLINE Global(v8::Local<S> that)
      : PersistentBase<T>(v8::Persistent<T>::New(that)) {
    TYPE_CHECK(T, S);
  }

  template <typename S>
  NAN_INLINE Global(const PersistentBase<S> &that)
    : PersistentBase<T>(that) {
    TYPE_CHECK(T, S);
  }
  /**
   * Move constructor.
   */
  NAN_INLINE Global(RValue rvalue)
    : PersistentBase<T>(rvalue.object->persistent) {
    rvalue.object->Reset();
  }
  NAN_INLINE ~Global() { this->Reset(); }
  /**
   * Move via assignment.
   */
  template<typename S>
  NAN_INLINE Global &operator=(Global<S> rhs) {
    TYPE_CHECK(T, S);
    this->Reset(rhs.persistent);
    rhs.Reset();
    return *this;
  }
  /**
   * Cast operator for moves.
   */
  NAN_INLINE operator RValue() { return RValue(this); }
  /**
   * Pass allows returning uniques from functions, etc.
   */
  Global Pass() { return Global(RValue(this)); }

 private:
  Global(Global &);
  void operator=(Global &);
  template<typename S> friend class ReturnValue;
};

#endif  // NAN_PERSISTENT_PRE_12_INL_H_
