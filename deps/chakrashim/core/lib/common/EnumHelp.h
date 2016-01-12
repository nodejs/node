//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// macro BEGIN_ENUM
///
/// BEGIN_ENUM is used to create a C++ 'struct' type around an enum, providing
/// a scope for the enum instead of being defined at the global namespace.
/// Combined with the helper macros of BEGIN_ENUM_BYTE(), ..., this enforces
/// that the enum will only be given the intended allocated storage instead of
/// the default storage of a full 4-byte 'int'.
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

#define BEGIN_ENUM(name, storage)           \
    struct name                             \
    {                                       \
        enum _E;                             \
                                            \
        inline name()                       \
        {                                   \
            _value = (storage) 0;            \
        }                                   \
                                            \
        inline name(_E src)                  \
        {                                   \
            _value = (storage) src;          \
        }                                   \
                                            \
        inline name(storage n)              \
        {                                   \
            _value = n;                      \
        }                                   \
                                            \
        inline name(int n)                  \
        {                                   \
            /*                                                                          */ \
            /* This is needed to enable operations such as "m_value &= ~Flags::Member;  */ \
            /*                                                                          */ \
                                            \
            _value = (storage) n;            \
            AssertMsg(((int) _value) == n, "Ensure no truncation"); \
        }                                   \
                                            \
        inline void operator =(_E e)         \
        {                                   \
            _value = (storage) e;            \
        }                                   \
                                            \
        inline void operator =(storage n)   \
        {                                   \
            _value = n;                      \
        }                                   \
                                            \
        inline bool operator ==(_E e) const  \
        {                                   \
            return ((_E) _value) == e;        \
        }                                   \
                                            \
        inline bool operator !=(_E e) const  \
        {                                   \
            return ((_E) _value) != e;        \
        }                                   \
                                            \
        inline bool operator <(_E e) const   \
        {                                   \
            return ((_E) _value) < e;         \
        }                                   \
                                            \
        inline bool operator <=(_E e) const  \
        {                                   \
            return ((_E) _value) <= e;        \
        }                                   \
                                            \
        inline bool operator >(_E e) const   \
        {                                   \
            return ((_E) _value) > e;         \
        }                                   \
                                            \
        inline bool operator >=(_E e) const  \
        {                                   \
            return ((_E) _value) >= e;        \
        }                                   \
                                            \
        inline _E operator &(_E e) const      \
        {                                   \
            return (_E) (((_E) _value) & e);   \
        }                                   \
                                            \
        inline _E operator |(name e) const   \
        {                                   \
            return (_E) (_value | e._value);   \
        }                                   \
                                            \
        inline void operator |=(name e)     \
        {                                   \
            _value = _value | e._value;        \
        }                                   \
                                            \
        inline void operator &=(name e)     \
        {                                   \
            _value = _value & e._value;        \
        }                                   \
                                            \
        inline void operator &=(_E e)        \
        {                                   \
            _value = _value & ((storage) e);  \
        }                                   \
                                            \
        inline operator _E() const           \
        {                                   \
            return (_E) _value;               \
        }                                   \
                                            \
        enum _E                              \
        {                                   \


#define BEGIN_ENUM_BYTE(name) BEGIN_ENUM(name, byte)
#define BEGIN_ENUM_USHORT(name) BEGIN_ENUM(name, uint16)
#define BEGIN_ENUM_UINT(name) BEGIN_ENUM(name, uint32)

#define END_ENUM_BYTE()                        \
            Force8BitPadding = (byte) 0xffU \
        };                                  \
                                            \
        byte _value;                         \
    };                                      \


#define END_ENUM_USHORT()                       \
            Force16BitPadding = (uint16) 0xffffU \
        };                                  \
                                            \
        uint16 _value;                       \
    };                                      \


#define END_ENUM_UINT()                       \
            Force32BitPadding = (uint32) 0xffffffffU \
        };                                  \
                                            \
        uint32 _value;                       \
    };


///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// macro PREVENT_ASSIGN
///
/// PREVENT_ASSIGN is used within a C++ type definition to define and
/// explicitly hide the "operator =()" method, preventing it from accidentally
/// being called by the program.  If these are not explicitly defined, the C++
/// compiler will implicitly define them, which usually leads to unintended
/// behavior.
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

#define PREVENT_ASSIGN(ClassName) \
    private: \
        ClassName & operator =(const ClassName & rhs);


///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// macro PREVENT_COPY
///
/// PREVENT_COPY is used within a C++ type definition to define and explicitly
/// hide the "C++ copy constructor" and "operator =()" methods, preventing them
/// from accidentally being called by the program.  If these are not explicitly
/// defined, the C++ compiler will implicitly define them, which usually leads
/// to unintended behavior.
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

#define PREVENT_COPYCONSTRUCT(ClassName) \
    private: \
        ClassName(const ClassName & copy);

#define PREVENT_COPY(ClassName) \
    PREVENT_COPYCONSTRUCT(ClassName); \
    PREVENT_ASSIGN(ClassName);


///----------------------------------------------------------------------------
///----------------------------------------------------------------------------
///
/// macro DECLARE_OBJECT
///
/// DECLARE_OBJECT sets up a class that derives from RcObject, RecyclableObject or
/// ZnObject:
/// - Prevent "C++ copy constructor" and "operator =()" methods.
/// - Must be allocated on heap.  Because the copy constructor is hidden, this
///   requires an empty default constructor to be declared.
///
///----------------------------------------------------------------------------
///----------------------------------------------------------------------------

#define DECLARE_OBJECT(ClassName) \
    public: \
        inline ClassName() { } \
    private: \
        ClassName(const ClassName & copy); \
        ClassName & operator =(const ClassName & rhs);
