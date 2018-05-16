#ifndef SRC_CORE_H_
#define SRC_CORE_H_

#ifdef _WIN32
# ifndef BUILDING_NODE_EXTENSION
#   define NODE_EXTERN __declspec(dllexport)
# else
#   define NODE_EXTERN __declspec(dllimport)
# endif
#else
# define NODE_EXTERN /* nothing */
#endif

#define NODE_MAKE_VERSION(major, minor, patch)                                \
  ((major) * 0x1000 + (minor) * 0x100 + (patch))

#ifdef __clang__
# define NODE_CLANG_AT_LEAST(major, minor, patch)                             \
  (NODE_MAKE_VERSION(major, minor, patch) <=                                  \
      NODE_MAKE_VERSION(__clang_major__, __clang_minor__, __clang_patchlevel__))
#else
# define NODE_CLANG_AT_LEAST(major, minor, patch) (0)
#endif

#ifdef __GNUC__
# define NODE_GNUC_AT_LEAST(major, minor, patch)                              \
  (NODE_MAKE_VERSION(major, minor, patch) <=                                  \
      NODE_MAKE_VERSION(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__))
#else
# define NODE_GNUC_AT_LEAST(major, minor, patch) (0)
#endif

#if NODE_CLANG_AT_LEAST(2, 9, 0) || NODE_GNUC_AT_LEAST(4, 5, 0)
# define NODE_DEPRECATED(message, declarator)                                 \
    __attribute__((deprecated(message))) declarator
#elif defined(_MSC_VER)
# define NODE_DEPRECATED(message, declarator)                                 \
    __declspec(deprecated) declarator
#else
# define NODE_DEPRECATED(message, declarator)                                 \
    declarator
#endif

#endif  // SRC_CORE_H_
