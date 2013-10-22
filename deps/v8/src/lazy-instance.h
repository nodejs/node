// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

// The LazyInstance<Type, Traits> class manages a single instance of Type,
// which will be lazily created on the first time it's accessed.  This class is
// useful for places you would normally use a function-level static, but you
// need to have guaranteed thread-safety.  The Type constructor will only ever
// be called once, even if two threads are racing to create the object.  Get()
// and Pointer() will always return the same, completely initialized instance.
//
// LazyInstance is completely thread safe, assuming that you create it safely.
// The class was designed to be POD initialized, so it shouldn't require a
// static constructor.  It really only makes sense to declare a LazyInstance as
// a global variable using the LAZY_INSTANCE_INITIALIZER initializer.
//
// LazyInstance is similar to Singleton, except it does not have the singleton
// property.  You can have multiple LazyInstance's of the same type, and each
// will manage a unique instance.  It also preallocates the space for Type, as
// to avoid allocating the Type instance on the heap.  This may help with the
// performance of creating the instance, and reducing heap fragmentation.  This
// requires that Type be a complete type so we can determine the size. See
// notes for advanced users below for more explanations.
//
// Example usage:
//   static LazyInstance<MyClass>::type my_instance = LAZY_INSTANCE_INITIALIZER;
//   void SomeMethod() {
//     my_instance.Get().SomeMethod();  // MyClass::SomeMethod()
//
//     MyClass* ptr = my_instance.Pointer();
//     ptr->DoDoDo();  // MyClass::DoDoDo
//   }
//
// Additionally you can override the way your instance is constructed by
// providing your own trait:
// Example usage:
//   struct MyCreateTrait {
//     static void Construct(MyClass* allocated_ptr) {
//       new (allocated_ptr) MyClass(/* extra parameters... */);
//     }
//   };
//   static LazyInstance<MyClass, MyCreateTrait>::type my_instance =
//      LAZY_INSTANCE_INITIALIZER;
//
// WARNINGS:
// - This implementation of LazyInstance is NOT THREAD-SAFE by default. See
//   ThreadSafeInitOnceTrait declared below for that.
// - Lazy initialization comes with a cost. Make sure that you don't use it on
//   critical path. Consider adding your initialization code to a function
//   which is explicitly called once.
//
// Notes for advanced users:
// LazyInstance can actually be used in two different ways:
//
// - "Static mode" which is the default mode since it is the most efficient
//   (no extra heap allocation). In this mode, the instance is statically
//   allocated (stored in the global data section at compile time).
//   The macro LAZY_STATIC_INSTANCE_INITIALIZER (= LAZY_INSTANCE_INITIALIZER)
//   must be used to initialize static lazy instances.
//
// - "Dynamic mode". In this mode, the instance is dynamically allocated and
//   constructed (using new) by default. This mode is useful if you have to
//   deal with some code already allocating the instance for you (e.g.
//   OS::Mutex() which returns a new private OS-dependent subclass of Mutex).
//   The macro LAZY_DYNAMIC_INSTANCE_INITIALIZER must be used to initialize
//   dynamic lazy instances.

#ifndef V8_LAZY_INSTANCE_H_
#define V8_LAZY_INSTANCE_H_

#include "checks.h"
#include "once.h"

namespace v8 {
namespace internal {

#define LAZY_STATIC_INSTANCE_INITIALIZER { V8_ONCE_INIT, { {} } }
#define LAZY_DYNAMIC_INSTANCE_INITIALIZER { V8_ONCE_INIT, 0 }

// Default to static mode.
#define LAZY_INSTANCE_INITIALIZER LAZY_STATIC_INSTANCE_INITIALIZER


template <typename T>
struct LeakyInstanceTrait {
  static void Destroy(T* /* instance */) {}
};


// Traits that define how an instance is allocated and accessed.


template <typename T>
struct StaticallyAllocatedInstanceTrait {
  // 16-byte alignment fallback to be on the safe side here.
  struct V8_ALIGNAS(T, 16) StorageType {
    char x[sizeof(T)];
  };

  STATIC_ASSERT(V8_ALIGNOF(StorageType) >= V8_ALIGNOF(T));

  static T* MutableInstance(StorageType* storage) {
    return reinterpret_cast<T*>(storage);
  }

  template <typename ConstructTrait>
  static void InitStorageUsingTrait(StorageType* storage) {
    ConstructTrait::Construct(MutableInstance(storage));
  }
};


template <typename T>
struct DynamicallyAllocatedInstanceTrait {
  typedef T* StorageType;

  static T* MutableInstance(StorageType* storage) {
    return *storage;
  }

  template <typename CreateTrait>
  static void InitStorageUsingTrait(StorageType* storage) {
    *storage = CreateTrait::Create();
  }
};


template <typename T>
struct DefaultConstructTrait {
  // Constructs the provided object which was already allocated.
  static void Construct(T* allocated_ptr) {
    new(allocated_ptr) T();
  }
};


template <typename T>
struct DefaultCreateTrait {
  static T* Create() {
    return new T();
  }
};


struct ThreadSafeInitOnceTrait {
  template <typename Function, typename Storage>
  static void Init(OnceType* once, Function function, Storage storage) {
    CallOnce(once, function, storage);
  }
};


// Initialization trait for users who don't care about thread-safety.
struct SingleThreadInitOnceTrait {
  template <typename Function, typename Storage>
  static void Init(OnceType* once, Function function, Storage storage) {
    if (*once == ONCE_STATE_UNINITIALIZED) {
      function(storage);
      *once = ONCE_STATE_DONE;
    }
  }
};


// TODO(pliard): Handle instances destruction (using global destructors).
template <typename T, typename AllocationTrait, typename CreateTrait,
          typename InitOnceTrait, typename DestroyTrait  /* not used yet. */>
struct LazyInstanceImpl {
 public:
  typedef typename AllocationTrait::StorageType StorageType;

 private:
  static void InitInstance(StorageType* storage) {
    AllocationTrait::template InitStorageUsingTrait<CreateTrait>(storage);
  }

  void Init() const {
    InitOnceTrait::Init(
        &once_,
        // Casts to void* are needed here to avoid breaking strict aliasing
        // rules.
        reinterpret_cast<void(*)(void*)>(&InitInstance),  // NOLINT
        reinterpret_cast<void*>(&storage_));
  }

 public:
  T* Pointer() {
    Init();
    return AllocationTrait::MutableInstance(&storage_);
  }

  const T& Get() const {
    Init();
    return *AllocationTrait::MutableInstance(&storage_);
  }

  mutable OnceType once_;
  // Note that the previous field, OnceType, is an AtomicWord which guarantees
  // 4-byte alignment of the storage field below. If compiling with GCC (>4.2),
  // the LAZY_ALIGN macro above will guarantee correctness for any alignment.
  mutable StorageType storage_;
};


template <typename T,
          typename CreateTrait = DefaultConstructTrait<T>,
          typename InitOnceTrait = SingleThreadInitOnceTrait,
          typename DestroyTrait = LeakyInstanceTrait<T> >
struct LazyStaticInstance {
  typedef LazyInstanceImpl<T, StaticallyAllocatedInstanceTrait<T>,
      CreateTrait, InitOnceTrait, DestroyTrait> type;
};


template <typename T,
          typename CreateTrait = DefaultConstructTrait<T>,
          typename InitOnceTrait = SingleThreadInitOnceTrait,
          typename DestroyTrait = LeakyInstanceTrait<T> >
struct LazyInstance {
  // A LazyInstance is a LazyStaticInstance.
  typedef typename LazyStaticInstance<T, CreateTrait, InitOnceTrait,
      DestroyTrait>::type type;
};


template <typename T,
          typename CreateTrait = DefaultCreateTrait<T>,
          typename InitOnceTrait = SingleThreadInitOnceTrait,
          typename DestroyTrait = LeakyInstanceTrait<T> >
struct LazyDynamicInstance {
  typedef LazyInstanceImpl<T, DynamicallyAllocatedInstanceTrait<T>,
      CreateTrait, InitOnceTrait, DestroyTrait> type;
};

} }  // namespace v8::internal

#endif  // V8_LAZY_INSTANCE_H_
