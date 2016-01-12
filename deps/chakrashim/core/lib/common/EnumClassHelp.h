//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// This macro defines some global operators, and hence must be used at global scope
#define ENUM_CLASS_HELPERS(TEnum, TUnderlying) \
    inline TEnum operator +(const TEnum e, const TUnderlying n) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) + n); \
    } \
    \
    inline TEnum operator +(const TUnderlying n, const TEnum e) \
    { \
        return static_cast<TEnum>(n + static_cast<TUnderlying>(e)); \
    } \
    \
    inline TEnum operator +(const TEnum e0, const TEnum e1) \
    { \
        return static_cast<TUnderlying>(e0) + e1; \
    } \
    \
    inline TEnum &operator +=(TEnum &e, const TUnderlying n) \
    { \
        return e = e + n; \
    } \
    \
    inline TEnum &operator ++(TEnum &e) \
    { \
        return e += 1; \
    } \
    \
    inline TEnum operator ++(TEnum &e, const int) \
    { \
        const TEnum old = e; \
        ++e; \
        return old; \
    } \
    \
    inline TEnum operator -(const TEnum e, const TUnderlying n) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) - n); \
    } \
    \
    inline TEnum operator -(const TUnderlying n, const TEnum e) \
    { \
        return static_cast<TEnum>(n - static_cast<TUnderlying>(e)); \
    } \
    \
    inline TEnum operator -(const TEnum e0, const TEnum e1) \
    { \
        return static_cast<TUnderlying>(e0) - e1; \
    } \
    \
    inline TEnum &operator -=(TEnum &e, const TUnderlying n) \
    { \
        return e = e - n; \
    } \
    \
    inline TEnum &operator --(TEnum &e) \
    { \
        return e -= 1; \
    } \
    \
    inline TEnum operator --(TEnum &e, const int) \
    { \
        const TEnum old = e; \
        --e; \
        return old; \
    } \
    \
    inline TEnum operator &(const TEnum e0, const TEnum e1) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(e0) & static_cast<TUnderlying>(e1)); \
    } \
    \
    inline TEnum &operator &=(TEnum &e0, const TEnum e1) \
    { \
        return e0 = e0 & e1; \
    } \
    \
    inline TEnum operator ^(const TEnum e0, const TEnum e1) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(e0) ^ static_cast<TUnderlying>(e1)); \
    } \
    \
    inline TEnum &operator ^=(TEnum &e0, const TEnum e1) \
    { \
        return e0 = e0 ^ e1; \
    } \
    \
    inline TEnum operator |(const TEnum e0, const TEnum e1) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(e0) | static_cast<TUnderlying>(e1)); \
    } \
    \
    inline TEnum &operator |=(TEnum &e0, const TEnum e1) \
    { \
        return e0 = e0 | e1; \
    } \
    \
    inline TEnum operator <<(const TEnum e, const TUnderlying n) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) << n); \
    } \
    \
    inline TEnum &operator <<=(TEnum &e, const TUnderlying n) \
    { \
        return e = e << n; \
    } \
    \
    inline TEnum operator >>(const TEnum e, const TUnderlying n) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(e) >> n); \
    } \
    \
    inline TEnum &operator >>=(TEnum &e, const TUnderlying n) \
    { \
        return e = e >> n; \
    } \
    \
    inline TEnum operator ~(const TEnum e) \
    { \
        return static_cast<TEnum>(~static_cast<TUnderlying>(e)); \
    } \
    \
    inline TEnum operator +(const TEnum e) \
    { \
        return e; \
    } \
    \
    inline TEnum operator -(const TEnum e) \
    { \
        return static_cast<TEnum>(static_cast<TUnderlying>(0) - static_cast<TUnderlying>(e)); \
    } \
    \
    inline bool operator !(const TEnum e) \
    { \
        return !static_cast<TUnderlying>(e); \
    } \

// For private enum classes defined inside other classes, this macro can be used inside the class to declare friends
#define ENUM_CLASS_HELPER_FRIENDS(TEnum, TUnderlying) \
    friend TEnum operator +(const TEnum e, const TUnderlying n); \
    friend TEnum operator +(const TUnderlying n, const TEnum e); \
    friend TEnum operator +(const TEnum e0, const TEnum e1); \
    friend TEnum &operator +=(TEnum &e, const TUnderlying n); \
    friend TEnum &operator ++(TEnum &e); \
    friend TEnum operator ++(TEnum &e, const int); \
    friend TEnum operator -(const TEnum e, const TUnderlying n); \
    friend TEnum operator -(const TUnderlying n, const TEnum e); \
    friend TEnum operator -(const TEnum e0, const TEnum e1); \
    friend TEnum &operator -=(TEnum &e, const TUnderlying n); \
    friend TEnum &operator --(TEnum &e); \
    friend TEnum operator --(TEnum &e, const int); \
    friend TEnum operator &(const TEnum e0, const TEnum e1); \
    friend TEnum &operator &=(TEnum &e0, const TEnum e1); \
    friend TEnum operator ^(const TEnum e0, const TEnum e1); \
    friend TEnum &operator ^=(TEnum &e0, const TEnum e1); \
    friend TEnum operator |(const TEnum e0, const TEnum e1); \
    friend TEnum &operator |=(TEnum &e0, const TEnum e1); \
    friend TEnum operator <<(const TEnum e, const TUnderlying n); \
    friend TEnum &operator <<=(TEnum &e, const TUnderlying n); \
    friend TEnum operator >>(const TEnum e, const TUnderlying n); \
    friend TEnum &operator >>=(TEnum &e, const TUnderlying n); \
    friend TEnum operator ~(const TEnum e); \
    friend TEnum operator +(const TEnum e); \
    friend TEnum operator -(const TEnum e); \
    friend bool operator !(const TEnum e);
