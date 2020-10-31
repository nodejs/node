# We check two things: where the include file is for
# unordered_map/hash_map (we prefer the first form), and what
# namespace unordered/hash_map lives in within that include file.  We
# include AC_TRY_COMPILE for all the combinations we've seen in the
# wild.  We define HASH_MAP_H to the location of the header file, and
# HASH_NAMESPACE to the namespace the class (unordered_map or
# hash_map) is in.

# This also checks if unordered map exists.
AC_DEFUN([AC_CXX_STL_HASH],
  [
   AC_MSG_CHECKING(the location of hash_map)
   AC_LANG_SAVE
   AC_LANG_CPLUSPLUS
   ac_cv_cxx_hash_map=""
   # First try unordered_map, but not on gcc's before 4.2 -- I've
   # seen unexplainable unordered_map bugs with -O2 on older gcc's.
   AC_TRY_COMPILE([#if defined(__GNUC__) && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 2))
                   # error GCC too old for unordered_map
                   #endif
                   ],
                   [/* no program body necessary */],
                   [stl_hash_old_gcc=no],
                   [stl_hash_old_gcc=yes])
   for location in unordered_map tr1/unordered_map; do
     for namespace in std std::tr1; do
       if test -z "$ac_cv_cxx_hash_map" -a "$stl_hash_old_gcc" != yes; then
         # Some older gcc's have a buggy tr1, so test a bit of code.
         AC_TRY_COMPILE([#include <$location>],
                        [const ${namespace}::unordered_map<int, int> t;
                         return t.find(5) == t.end();],
                        [ac_cv_cxx_hash_map="<$location>";
                         ac_cv_cxx_hash_namespace="$namespace";
                         ac_cv_cxx_hash_map_class="unordered_map";])
       fi
     done
   done
   # Now try hash_map
   for location in ext/hash_map hash_map; do
     for namespace in __gnu_cxx "" std stdext; do
       if test -z "$ac_cv_cxx_hash_map"; then
         AC_TRY_COMPILE([#include <$location>],
                        [${namespace}::hash_map<int, int> t],
                        [ac_cv_cxx_hash_map="<$location>";
                         ac_cv_cxx_hash_namespace="$namespace";
                         ac_cv_cxx_hash_map_class="hash_map";])
       fi
     done
   done
   ac_cv_cxx_hash_set=`echo "$ac_cv_cxx_hash_map" | sed s/map/set/`;
   ac_cv_cxx_hash_set_class=`echo "$ac_cv_cxx_hash_map_class" | sed s/map/set/`;
   if test -n "$ac_cv_cxx_hash_map"; then
      AC_DEFINE(HAVE_HASH_MAP, 1, [define if the compiler has hash_map])
      AC_DEFINE(HAVE_HASH_SET, 1, [define if the compiler has hash_set])
      AC_DEFINE_UNQUOTED(HASH_MAP_H,$ac_cv_cxx_hash_map,
                         [the location of <unordered_map> or <hash_map>])
      AC_DEFINE_UNQUOTED(HASH_SET_H,$ac_cv_cxx_hash_set,
                         [the location of <unordered_set> or <hash_set>])
      AC_DEFINE_UNQUOTED(HASH_NAMESPACE,$ac_cv_cxx_hash_namespace,
                         [the namespace of hash_map/hash_set])
      AC_DEFINE_UNQUOTED(HASH_MAP_CLASS,$ac_cv_cxx_hash_map_class,
                         [the name of <hash_map>])
      AC_DEFINE_UNQUOTED(HASH_SET_CLASS,$ac_cv_cxx_hash_set_class,
                         [the name of <hash_set>])
      AC_MSG_RESULT([$ac_cv_cxx_hash_map])
   else
      AC_MSG_RESULT()
      AC_MSG_WARN([could not find an STL hash_map])
   fi
])

