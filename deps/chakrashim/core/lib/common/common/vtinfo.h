//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// These definitions allow us to programmatically access a class' vtable for:
//
// 1. Cross-context marshalling: when an object needs to be marshalled, we
//    replace its vtable with another that will force the marshalling
//
// 2. Heap enumeration: for any instance that we want to report, basically
//    everything that inherits from RecyclableObject, we register the class'
//    vtable with the enumeration code. This allows us to compare a GC heap
//    address against the vtable list to determine whether it's a type we care about
//    (we don't distinguish between RecyclableObjects and other types
//    of storage in the GC heap).
//----------------------------------------------------------------------------

#pragma once

enum VirtualTableInfoCtorEnum
{
    VirtualTableInfoCtorValue
};

#define MAX_KNOWN_VTABLES 300

#if DBG
#ifdef HEAP_ENUMERATION_VALIDATION
#define ENABLE_VALIDATE_VTABLE_CTOR 1
#define HEAP_ENUMERATION_LIBRARY_OBJECT_COOKIE -1
#endif
#endif

#ifdef ENABLE_VALIDATE_VTABLE_CTOR
#define POST_ALLOCATION_VALIDATION 1

// PostAllocationCallback allows us to validate that an object derived from a known
// base class has included DEFINE_VTABLE_CTOR. Although this could be statically determined,
// the language doesn't support a way to enforce that, so we do it at runtime on object allocation.
//
// Any class that includes DEFINE_VTABLE_CTOR will also override ValidateVtableRegistered() function
// with a type-specific version that compares the vtable of the type against VirtualTableInfo<T>::Address.
// Any class that doesn't include DEFINE_VTABLE_CTOR will pick up its parent's version of ValidateVtableRegistered
// and the type-specific vtable comparison will fail.

#define DEFINE_VALIDATE_HAS_VTABLE_CTOR(T) \
    void PostAllocationCallbackForHeapEnumValidation(const type_info& objType, Js::DynamicObject* obj) \
    { \
        if (Js::Configuration::Global.flags.ValidateHeapEnum) \
        { \
            ((Js::RecyclableObject*)obj)->ValidateVtableRegistered(objType); \
        } \
        Js::JavascriptLibrary *library = obj->GetLibrary(); \
        if (! library || ! library->GetScriptContext()->IsInitialized()) \
        { \
            obj->SetHeapEnumValidationCookie(HEAP_ENUMERATION_LIBRARY_OBJECT_COOKIE); \
        } \
    } \

#define DECLARE_VALIDATE_VTABLE_REGISTERED_NOBASE_ABSTRACT(T) \
    virtual void ValidateVtableRegistered(const type_info& objType) = 0;

#define VALIDATE_VTABLE_REGISTERED_BODY(T) \
    { \
        if (typeid(T) != objType) \
        { \
            AssertMsg(typeid(T) == objType, "Class derived from Js::RecyclableObject missing DEFINE_VTABLE_CTOR"); \
            Output::Print(L"%S missing DEFINE_VTABLE_CTOR\n", objType.name()); \
        } \
    }

#define DECLARE_VALIDATE_VTABLE_REGISTERED_NOBASE(T) \
    virtual void ValidateVtableRegistered(const type_info& objType) VALIDATE_VTABLE_REGISTERED_BODY(T) \

#define DEFINE_VALIDATE_VTABLE_REGISTERED(T) \
    void ValidateVtableRegistered(const type_info& objType) override VALIDATE_VTABLE_REGISTERED_BODY(T)

#else
#define DEFINE_VALIDATE_HAS_VTABLE_CTOR(T)
#define DECLARE_VALIDATE_VTABLE_REGISTERED_NOBASE_ABSTRACT(T)
#define DECLARE_VALIDATE_VTABLE_REGISTERED_NOBASE(T)
#define DEFINE_VALIDATE_VTABLE_REGISTERED(T)
#define VALIDATE_HAS_VTABLE_CTOR(T, obj)
#endif

class VirtualTableInfoBase
{
public:
    static INT_PTR GetVirtualTable(void * ptr) { return (*(INT_PTR*)ptr); }
};

template <typename T>
class VirtualTableInfo : public VirtualTableInfoBase
{
public:
    static INT_PTR const Address;
    static INT_PTR RegisterVirtualTable(INT_PTR vtable);
    static void SetVirtualTable(void * ptr) { new (ptr) T(VirtualTableInfoCtorValue); }
    static bool HasVirtualTable(void * ptr) { return GetVirtualTable(ptr) == Address; }
};

#if !defined(USED_IN_STATIC_LIB)
#pragma warning(disable:4238) // class rvalue used as lvalue
template <typename T>
INT_PTR const VirtualTableInfo<T>::Address = VirtualTableInfo<T>::RegisterVirtualTable(*(INT_PTR const*)&T(VirtualTableInfoCtorValue));
#endif

#define DEFINE_VTABLE_CTOR_NOBASE_ABSTRACT(T) \
    T(VirtualTableInfoCtorEnum v) {} \
    enum RegisterVTableEnum { RegisterVTable = 1 };

#define DEFINE_VTABLE_CTOR_NOBASE(T) \
    friend class VirtualTableInfo<T>; \
    DEFINE_VTABLE_CTOR_NOBASE_ABSTRACT(T)

#define DEFINE_VTABLE_CTOR_ABSTRACT(T, Base, ...) \
    T(VirtualTableInfoCtorEnum v) : Base(v), __VA_ARGS__ {}

#define DEFINE_VTABLE_CTOR(T, Base) \
    friend class VirtualTableInfo<T>; \
    DEFINE_VTABLE_CTOR_ABSTRACT(T, Base) \
    DEFINE_VALIDATE_VTABLE_REGISTERED(T);

// Used by non-RecyclableObject
#define DEFINE_VTABLE_CTOR_NO_REGISTER(T, Base, ...) \
    friend class VirtualTableInfo<T>; \
    DEFINE_VTABLE_CTOR_ABSTRACT(T, Base, __VA_ARGS__) \
    enum RegisterVTableEnum { RegisterVTable = 0 };

#define DEFINE_VTABLE_CTOR_MEMBER_INIT(T, Base, Member) \
    friend class VirtualTableInfo<T>; \
    T(VirtualTableInfoCtorEnum v) : Base(v), Member(v) {} \
    DEFINE_VALIDATE_VTABLE_REGISTERED(T);


#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#define DEFINE_GETCPPNAME_ABSTRACT() \
    virtual const char* GetCppName() const = 0;
#define DEFINE_GETCPPNAME() \
    virtual const char* GetCppName() const { return typeid(this).name(); }
#else
#define DEFINE_GETCPPNAME_ABSTRACT()
#define DEFINE_GETCPPNAME()
#endif
