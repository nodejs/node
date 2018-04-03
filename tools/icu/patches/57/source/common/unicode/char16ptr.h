// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// char16ptr.h
// created: 2017feb28 Markus W. Scherer
#ifndef __CHAR16PTR_H__
#define __CHAR16PTR_H__
#include <cstddef>
#include "unicode/utypes.h"
/**
 * \file
 * \brief C++ API: char16_t pointer wrappers with
 *        implicit conversion from bit-compatible raw pointer types.
 *        Also conversion functions from char16_t * to UChar * and OldUChar *.
 */
U_NAMESPACE_BEGIN
/**
 * \def U_ALIASING_BARRIER
 * Barrier for pointer anti-aliasing optimizations even across function boundaries.
 * @internal
 */
#ifdef U_ALIASING_BARRIER
    // Use the predefined value.
#elif (defined(__clang__) || defined(__GNUC__)) && U_PLATFORM != U_PF_BROWSER_NATIVE_CLIENT
#   define U_ALIASING_BARRIER(ptr) asm volatile("" : : "rm"(ptr) : "memory")
#endif
// Do not use #ifndef U_HIDE_DRAFT_API for the following class, it
// is now used in place of UChar* in several stable C++ methods
/**
 * char16_t * wrapper with implicit conversion from distinct but bit-compatible pointer types.
 * @draft ICU 59
 */
class U_COMMON_API Char16Ptr U_FINAL {
public:
    /**
     * Copies the pointer.
     * @param p pointer
     * @draft ICU 59
     */
    inline Char16Ptr(char16_t *p);
#if !U_CHAR16_IS_TYPEDEF
    /**
     * Converts the pointer to char16_t *.
     * @param p pointer to be converted
     * @draft ICU 59
     */
    inline Char16Ptr(uint16_t *p);
#endif
#if U_SIZEOF_WCHAR_T==2 || defined(U_IN_DOXYGEN)
    /**
     * Converts the pointer to char16_t *.
     * (Only defined if U_SIZEOF_WCHAR_T==2.)
     * @param p pointer to be converted
     * @draft ICU 59
     */
    inline Char16Ptr(wchar_t *p);
#endif
    /**
     * nullptr constructor.
     * @param p nullptr
     * @draft ICU 59
     */
    inline Char16Ptr(std::nullptr_t p);
    /**
     * Destructor.
     * @draft ICU 59
     */
    inline ~Char16Ptr();
    /**
     * Pointer access.
     * @return the wrapped pointer
     * @draft ICU 59
     */
    inline UChar *get() const;
    /**
     * char16_t pointer access via type conversion (e.g., static_cast).
     * @return the wrapped pointer
     * @draft ICU 59
     */
    inline operator UChar *() const { return get(); }
private:
    Char16Ptr() = delete;
#ifdef U_ALIASING_BARRIER
    template<typename T> static char16_t *cast(T *t) {
        U_ALIASING_BARRIER(t);
        return reinterpret_cast<char16_t *>(t);
    }
    char16_t *p;
#else
    union {
        char16_t *cp;
        uint16_t *up;
        wchar_t *wp;
        UChar    *ucp;
    } u;
#endif
};
#ifdef U_ALIASING_BARRIER
Char16Ptr::Char16Ptr(char16_t *p) : p(p) {}
#if !U_CHAR16_IS_TYPEDEF
Char16Ptr::Char16Ptr(uint16_t *p) : p(cast(p)) {}
#endif
#if U_SIZEOF_WCHAR_T==2
Char16Ptr::Char16Ptr(wchar_t *p) : p(cast(p)) {}
#endif
Char16Ptr::Char16Ptr(std::nullptr_t p) : p(p) {}
Char16Ptr::~Char16Ptr() {
    U_ALIASING_BARRIER(p);
}
UChar *Char16Ptr::get() const {
#ifdef U_ALIASING_BARRIER
    U_ALIASING_BARRIER(p);
#endif
    return reinterpret_cast<UChar *>(p);
}
#else
Char16Ptr::Char16Ptr(char16_t *p) { u.cp = p; }
#if !U_CHAR16_IS_TYPEDEF
Char16Ptr::Char16Ptr(uint16_t *p) { u.up = p; }
#endif
#if U_SIZEOF_WCHAR_T==2
Char16Ptr::Char16Ptr(wchar_t *p) { u.wp = p; }
#endif
Char16Ptr::Char16Ptr(std::nullptr_t p) { u.cp = p; }
Char16Ptr::~Char16Ptr() {}
UChar *Char16Ptr::get() const { return u.ucp; }
#endif
// Do not use #ifndef U_HIDE_DRAFT_API for the following class, it is
// now used in place of const UChar* in several stable C++ methods
/**
 * const char16_t * wrapper with implicit conversion from distinct but bit-compatible pointer types.
 * @draft ICU 59
 */
class U_COMMON_API ConstChar16Ptr U_FINAL {
public:
    /**
     * Copies the pointer.
     * @param p pointer
     * @draft ICU 59
     */
    inline ConstChar16Ptr(const char16_t *p);
#if !U_CHAR16_IS_TYPEDEF
    /**
     * Converts the pointer to char16_t *.
     * @param p pointer to be converted
     * @draft ICU 59
     */
    inline ConstChar16Ptr(const uint16_t *p);
#endif
#if U_SIZEOF_WCHAR_T==2 || defined(U_IN_DOXYGEN)
    /**
     * Converts the pointer to char16_t *.
     * (Only defined if U_SIZEOF_WCHAR_T==2.)
     * @param p pointer to be converted
     * @draft ICU 59
     */
    inline ConstChar16Ptr(const wchar_t *p);
#endif
    /**
     * nullptr constructor.
     * @param p nullptr
     * @draft ICU 59
     */
    inline ConstChar16Ptr(const std::nullptr_t p);
    /**
     * Destructor.
     * @draft ICU 59
     */
    inline ~ConstChar16Ptr();
    /**
     * Pointer access.
     * @return the wrapped pointer
     * @draft ICU 59
     */
    inline const UChar *get() const;
    /**
     * char16_t pointer access via type conversion (e.g., static_cast).
     * @return the wrapped pointer
     * @draft ICU 59
     */
    inline operator const UChar *() const { return get(); }
private:
    ConstChar16Ptr() = delete;
#ifdef U_ALIASING_BARRIER
    template<typename T> static const char16_t *cast(const T *t) {
        U_ALIASING_BARRIER(t);
        return reinterpret_cast<const char16_t *>(t);
    }
    const char16_t *p;
#else
    union {
        const char16_t *cp;
        const uint16_t *up;
        const wchar_t *wp;
        Const UChar    *ucp;
    } u;
#endif
};
#ifdef U_ALIASING_BARRIER
ConstChar16Ptr::ConstChar16Ptr(const char16_t *p) : p(p) {}
#if !U_CHAR16_IS_TYPEDEF
ConstChar16Ptr::ConstChar16Ptr(const uint16_t *p) : p(cast(p)) {}
#endif
#if U_SIZEOF_WCHAR_T==2
ConstChar16Ptr::ConstChar16Ptr(const wchar_t *p) : p(cast(p)) {}
#endif
ConstChar16Ptr::ConstChar16Ptr(const std::nullptr_t p) : p(p) {}
ConstChar16Ptr::~ConstChar16Ptr() {
    U_ALIASING_BARRIER(p);
}
const UChar *ConstChar16Ptr::get() const {
#ifdef U_ALIASING_BARRIER
    U_ALIASING_BARRIER(p);
#endif
    return reinterpret_cast<const UChar *>(p);
}
#else
ConstChar16Ptr::ConstChar16Ptr(const char16_t *p) { u.cp = p; }
#if !U_CHAR16_IS_TYPEDEF
ConstChar16Ptr::ConstChar16Ptr(const uint16_t *p) { u.up = p; }
#endif
#if U_SIZEOF_WCHAR_T==2
ConstChar16Ptr::ConstChar16Ptr(const wchar_t *p) { u.wp = p; }
#endif
ConstChar16Ptr::ConstChar16Ptr(const std::nullptr_t p) { u.cp = p; }
ConstChar16Ptr::~ConstChar16Ptr() {}
const UChar *ConstChar16Ptr::get() const { return u.ucp; }
#endif
//  Patched in from ICU 59 umachine.h
/**
 * \var OldUChar
 * Default ICU 58 definition of UChar.
 * A base type for UTF-16 code units and pointers.
 * Unsigned 16-bit integer.
 *
 * Define OldUChar to be wchar_t if that is 16 bits wide.
 * If wchar_t is not 16 bits wide, then define UChar to be uint16_t.
 *
 * This makes the definition of OldUChar platform-dependent
 * but allows direct string type compatibility with platforms with
 * 16-bit wchar_t types.
 *
 * This is how UChar was defined in ICU 58, for transition convenience.
 * Exception: ICU 58 UChar was defined to UCHAR_TYPE if that macro was defined.
 * The current UChar responds to UCHAR_TYPE but OldUChar does not.
 *
 * @draft ICU 59
 */
#if U_SIZEOF_WCHAR_T==2
    typedef wchar_t OldUChar;
#elif defined(__CHAR16_TYPE__)
    typedef __CHAR16_TYPE__ OldUChar;
#else
    typedef uint16_t OldUChar;
#endif
//  End of patch from ICU 59 umachine.h
/**
 * Converts from const char16_t * to const UChar *.
 * Includes an aliasing barrier if available.
 * @param p pointer
 * @return p as const UChar *
 * @draft ICU 59
 */
inline const UChar *toUCharPtr(const UChar *p) {
  // backport: no-op
  return p;
}
/**
 * Converts from char16_t * to UChar *.
 * Includes an aliasing barrier if available.
 * @param p pointer
 * @return p as UChar *
 * @draft ICU 59
 */
inline UChar *toUCharPtr(UChar *p) {
  // backport: no-op
  return p;
}
/**
 * Converts from const char16_t * to const OldUChar *.
 * Includes an aliasing barrier if available.
 * @param p pointer
 * @return p as const OldUChar *
 * @draft ICU 59
 */
inline const OldUChar *toOldUCharPtr(const UChar *p) {
    return p;
}
/**
 * Converts from char16_t * to OldUChar *.
 * Includes an aliasing barrier if available.
 * @param p pointer
 * @return p as OldUChar *
 * @draft ICU 59
 */
inline OldUChar *toOldUCharPtr(UChar *p) {
    return p;
}
U_NAMESPACE_END
#endif  // __CHAR16PTR_H__
