// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_PROTOBUF_ARENASTRING_H__
#define GOOGLE_PROTOBUF_ARENASTRING_H__

#include <string>

#include <google/protobuf/arena.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/fastmem.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/port.h>

#include <google/protobuf/port_def.inc>

// This is the implementation of arena string fields written for the open-source
// release. The ArenaStringPtr struct below is an internal implementation class
// and *should not be used* by user code. It is used to collect string
// operations together into one place and abstract away the underlying
// string-field pointer representation, so that (for example) an alternate
// implementation that knew more about ::std::string's internals could integrate more
// closely with the arena allocator.

namespace google {
namespace protobuf {
namespace internal {

template <typename T>
class TaggedPtr {
 public:
  void Set(T* p) { ptr_ = reinterpret_cast<uintptr_t>(p); }
  T* Get() const { return reinterpret_cast<T*>(ptr_); }

  bool IsNull() { return ptr_ == 0; }

 private:
  uintptr_t ptr_;
};

struct PROTOBUF_EXPORT ArenaStringPtr {
  inline void Set(const ::std::string* default_value,
                  const ::std::string& value, Arena* arena) {
    if (ptr_ == default_value) {
      CreateInstance(arena, &value);
    } else {
      *ptr_ = value;
    }
  }

  inline void SetLite(const ::std::string* default_value,
                      const ::std::string& value, Arena* arena) {
    Set(default_value, value, arena);
  }

  // Basic accessors.
  inline const ::std::string& Get() const { return *ptr_; }

  inline ::std::string* Mutable(const ::std::string* default_value,
                                Arena* arena) {
    if (ptr_ == default_value) {
      CreateInstance(arena, default_value);
    }
    return ptr_;
  }

  // Release returns a ::std::string* instance that is heap-allocated and is not
  // Own()'d by any arena. If the field was not set, it returns NULL. The caller
  // retains ownership. Clears this field back to NULL state. Used to implement
  // release_<field>() methods on generated classes.
  inline ::std::string* Release(const ::std::string* default_value,
                                Arena* arena) {
    if (ptr_ == default_value) {
      return NULL;
    }
    return ReleaseNonDefault(default_value, arena);
  }

  // Similar to Release, but ptr_ cannot be the default_value.
  inline ::std::string* ReleaseNonDefault(const ::std::string* default_value,
                                          Arena* arena) {
    GOOGLE_DCHECK(!IsDefault(default_value));
    ::std::string* released = NULL;
    if (arena != NULL) {
      // ptr_ is owned by the arena.
      released = new ::std::string;
      released->swap(*ptr_);
    } else {
      released = ptr_;
    }
    ptr_ = const_cast< ::std::string* >(default_value);
    return released;
  }

  // UnsafeArenaRelease returns a ::std::string*, but it may be arena-owned (i.e.
  // have its destructor already registered) if arena != NULL. If the field was
  // not set, this returns NULL. This method clears this field back to NULL
  // state. Used to implement unsafe_arena_release_<field>() methods on
  // generated classes.
  inline ::std::string* UnsafeArenaRelease(const ::std::string* default_value,
                                           Arena* /* arena */) {
    if (ptr_ == default_value) {
      return NULL;
    }
    ::std::string* released = ptr_;
    ptr_ = const_cast< ::std::string* >(default_value);
    return released;
  }

  // Takes a string that is heap-allocated, and takes ownership. The string's
  // destructor is registered with the arena. Used to implement
  // set_allocated_<field> in generated classes.
  inline void SetAllocated(const ::std::string* default_value,
                           ::std::string* value, Arena* arena) {
    if (arena == NULL && ptr_ != default_value) {
      Destroy(default_value, arena);
    }
    if (value != NULL) {
      ptr_ = value;
      if (arena != NULL) {
        arena->Own(value);
      }
    } else {
      ptr_ = const_cast< ::std::string* >(default_value);
    }
  }

  // Takes a string that has lifetime equal to the arena's lifetime. The arena
  // must be non-null. It is safe only to pass this method a value returned by
  // UnsafeArenaRelease() on another field of a message in the same arena. Used
  // to implement unsafe_arena_set_allocated_<field> in generated classes.
  inline void UnsafeArenaSetAllocated(const ::std::string* default_value,
                                      ::std::string* value,
                                      Arena* /* arena */) {
    if (value != NULL) {
      ptr_ = value;
    } else {
      ptr_ = const_cast< ::std::string* >(default_value);
    }
  }

  // Swaps internal pointers. Arena-safety semantics: this is guarded by the
  // logic in Swap()/UnsafeArenaSwap() at the message level, so this method is
  // 'unsafe' if called directly.
  PROTOBUF_ALWAYS_INLINE void Swap(ArenaStringPtr* other) {
    std::swap(ptr_, other->ptr_);
  }
  PROTOBUF_ALWAYS_INLINE void Swap(ArenaStringPtr* other,
                                   const ::std::string* default_value,
                                   Arena* arena) {
#ifndef NDEBUG
    // For debug builds, we swap the contents of the string, rather than the
    // string instances themselves.  This invalidates previously taken const
    // references that are (per our documentation) invalidated by calling Swap()
    // on the message.
    //
    // If both strings are the default_value, swapping is uninteresting.
    // Otherwise, we use ArenaStringPtr::Mutable() to access the string, to
    // ensure that we do not try to mutate default_value itself.
    if (IsDefault(default_value) && other->IsDefault(default_value)) {
      return;
    }

    ::std::string* this_ptr = Mutable(default_value, arena);
    ::std::string* other_ptr = other->Mutable(default_value, arena);

    this_ptr->swap(*other_ptr);
#else
    std::swap(ptr_, other->ptr_);
    (void)default_value;
    (void)arena;
#endif
  }

  // Frees storage (if not on an arena).
  inline void Destroy(const ::std::string* default_value, Arena* arena) {
    if (arena == NULL && ptr_ != default_value) {
      delete ptr_;
    }
  }

  // Clears content, but keeps allocated string if arena != NULL, to avoid the
  // overhead of heap operations. After this returns, the content (as seen by
  // the user) will always be the empty string. Assumes that |default_value|
  // is an empty string.
  inline void ClearToEmpty(const ::std::string* default_value,
                           Arena* /* arena */) {
    if (ptr_ == default_value) {
      // Already set to default (which is empty) -- do nothing.
    } else {
      ptr_->clear();
    }
  }

  // Clears content, assuming that the current value is not the empty string
  // default.
  inline void ClearNonDefaultToEmpty() {
    ptr_->clear();
  }
  inline void ClearNonDefaultToEmptyNoArena() {
    ptr_->clear();
  }

  // Clears content, but keeps allocated string if arena != NULL, to avoid the
  // overhead of heap operations. After this returns, the content (as seen by
  // the user) will always be equal to |default_value|.
  inline void ClearToDefault(const ::std::string* default_value,
                             Arena* /* arena */) {
    if (ptr_ == default_value) {
      // Already set to default -- do nothing.
    } else {
      // Have another allocated string -- rather than throwing this away and
      // resetting ptr_ to the canonical default string instance, we just reuse
      // this instance.
      *ptr_ = *default_value;
    }
  }

  // Called from generated code / reflection runtime only. Resets value to point
  // to a default string pointer, with the semantics that this ArenaStringPtr
  // does not own the pointed-to memory. Disregards initial value of ptr_ (so
  // this is the *ONLY* safe method to call after construction or when
  // reinitializing after becoming the active field in a oneof union).
  inline void UnsafeSetDefault(const ::std::string* default_value) {
    // Casting away 'const' is safe here: accessors ensure that ptr_ is only
    // returned as a const if it is equal to default_value.
    ptr_ = const_cast< ::std::string* >(default_value);
  }

  // The 'NoArena' variants of methods below assume arena == NULL and are
  // optimized to provide very little overhead relative to a raw string pointer
  // (while still being in-memory compatible with other code that assumes
  // ArenaStringPtr). Note the invariant that a class instance that has only
  // ever been mutated by NoArena methods must *only* be in the String state
  // (i.e., tag bits are not used), *NEVER* ArenaString. This allows all
  // tagged-pointer manipulations to be avoided.
  inline void SetNoArena(const ::std::string* default_value,
                         const ::std::string& value) {
    if (ptr_ == default_value) {
      CreateInstanceNoArena(&value);
    } else {
      *ptr_ = value;
    }
  }

#if LANG_CXX11
  void SetNoArena(const ::std::string* default_value, ::std::string&& value) {
    if (IsDefault(default_value)) {
      ptr_ = new ::std::string(std::move(value));
    } else {
      *ptr_ = std::move(value);
    }
  }
#endif

  void AssignWithDefault(const ::std::string* default_value, ArenaStringPtr value);

  inline const ::std::string& GetNoArena() const { return *ptr_; }

  inline ::std::string* MutableNoArena(const ::std::string* default_value) {
    if (ptr_ == default_value) {
      CreateInstanceNoArena(default_value);
    }
    return ptr_;
  }

  inline ::std::string* ReleaseNoArena(const ::std::string* default_value) {
    if (ptr_ == default_value) {
      return NULL;
    } else {
      return ReleaseNonDefaultNoArena(default_value);
    }
  }

  inline ::std::string* ReleaseNonDefaultNoArena(
      const ::std::string* default_value) {
    GOOGLE_DCHECK(!IsDefault(default_value));
    ::std::string* released = ptr_;
    ptr_ = const_cast< ::std::string* >(default_value);
    return released;
  }


  inline void SetAllocatedNoArena(const ::std::string* default_value,
                                  ::std::string* value) {
    if (ptr_ != default_value) {
      delete ptr_;
    }
    if (value != NULL) {
      ptr_ = value;
    } else {
      ptr_ = const_cast< ::std::string* >(default_value);
    }
  }

  inline void DestroyNoArena(const ::std::string* default_value) {
    if (ptr_ != default_value) {
      delete ptr_;
    }
  }

  inline void ClearToEmptyNoArena(const ::std::string* default_value) {
    if (ptr_ == default_value) {
      // Nothing: already equal to default (which is the empty string).
    } else {
      ptr_->clear();
    }
  }

  inline void ClearToDefaultNoArena(const ::std::string* default_value) {
    if (ptr_ == default_value) {
      // Nothing: already set to default.
    } else {
      // Reuse existing allocated instance.
      *ptr_ = *default_value;
    }
  }

  // Internal accessor used only at parse time to provide direct access to the
  // raw pointer from the shared parse routine (in the non-arenas case). The
  // parse routine does the string allocation in order to save code size in the
  // generated parsing code.
  inline ::std::string** UnsafeRawStringPointer() {
    return &ptr_;
  }

  inline bool IsDefault(const ::std::string* default_value) const {
    return ptr_ == default_value;
  }

  // Internal accessors!!!!
  void UnsafeSetTaggedPointer(TaggedPtr< ::std::string> value) {
    ptr_ = value.Get();
  }
  // Generated code only! An optimization, in certain cases the generated
  // code is certain we can obtain a string with no default checks and
  // tag tests.
  ::std::string* UnsafeMutablePointer() { return ptr_; }

 private:
  ::std::string* ptr_;

  PROTOBUF_NOINLINE
  void CreateInstance(Arena* arena, const ::std::string* initial_value) {
    GOOGLE_DCHECK(initial_value != NULL);
    // uses "new ::std::string" when arena is nullptr
    ptr_ = Arena::Create< ::std::string >(arena, *initial_value);
  }
  PROTOBUF_NOINLINE
  void CreateInstanceNoArena(const ::std::string* initial_value) {
    GOOGLE_DCHECK(initial_value != NULL);
    ptr_ = new ::std::string(*initial_value);
  }
};

}  // namespace internal
}  // namespace protobuf



namespace protobuf {
namespace internal {

inline void ArenaStringPtr::AssignWithDefault(const ::std::string* default_value,
                                       ArenaStringPtr value) {
  const ::std::string* me = *UnsafeRawStringPointer();
  const ::std::string* other = *value.UnsafeRawStringPointer();
  // If the pointers are the same then do nothing.
  if (me != other) {
    SetNoArena(default_value, value.GetNoArena());
  }
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_ARENASTRING_H__
