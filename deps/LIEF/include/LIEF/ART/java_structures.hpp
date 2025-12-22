/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_ART_JAVA_STRUCTURES_H
#define LIEF_ART_JAVA_STRUCTURES_H

#include <cstring>
#include <tuple>

#include "LIEF/types.hpp"
#include "LIEF/ART/enums.hpp"
#include "LIEF/ART/types.hpp"

namespace LIEF {
/// Namespace related to the LIEF's ART module
namespace ART {

namespace details {

struct no_brooks_read_barrier_t {};

// ======================
// Android 6.0.1 - ART 17
// ======================
namespace ART_17 {

/// Namespace related to the Java part of ART 17
namespace Java {

using heap_reference_t = uint32_t;

struct brooks_read_barrier_t {
  uint32_t x_rb_ptr;
  uint32_t x_xpadding;
};

template<class T>
struct jobject_t {
  heap_reference_t klass;
  uint32_t         monitor;
  T                brooks_read_barrier;
};

template<>
struct jobject_t<no_brooks_read_barrier_t> {
  heap_reference_t klass;
  uint32_t         monitor;
};
template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jarray_t {
  jobject_t<T> object;
  int32_t   length;
  uint32_t* elements;
};

template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jclass_t {
  jobject_t<T>     object;

  heap_reference_t class_loader;
  heap_reference_t component_type;
  heap_reference_t dex_cache;
  heap_reference_t dex_cache_strings;
  heap_reference_t iftable;
  heap_reference_t name;
  heap_reference_t super_class;
  heap_reference_t verify_error_class;
  heap_reference_t vtable;

  uint32_t access_flags;
  uint64_t direct_methods;
  uint64_t ifields;
  uint64_t sfields;
  uint64_t virtual_methods;
  uint32_t class_size;
  uint32_t clinit_thread_id;
  int32_t  dex_class_def_idx;
  int32_t  dex_type_idx;
  uint32_t num_direct_methods;
  uint32_t num_instance_fields;
  uint32_t num_reference_instance_fields;
  uint32_t num_reference_static_fields;
  uint32_t num_static_fields;
  uint32_t num_virtual_methods;
  uint32_t object_size;
  uint32_t primitive_type;
  uint32_t reference_instance_offsets;
  int32_t  status;
};

template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jstring_t {
  jobject_t<T> object;
  int32_t      count;
  uint32_t     hash_code;
  uint16_t*    value;
};

template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jdex_cache_t {
  jobject_t<T> object;

  heap_reference_t dex;
  heap_reference_t location;
  heap_reference_t resolved_fields;
  heap_reference_t resolved_methods;
  heap_reference_t resolved_types;
  heap_reference_t strings;
  uint64_t         dex_file;
};


} // Namespace Java
} // Namespace ART_17

// ======================
// Android 7.0.0 - ART 29
// ======================
namespace ART_29 {

/// Namespace related to the Java part of ART 29
namespace Java {
using heap_reference_t      = ART_17::Java::heap_reference_t;
using brooks_read_barrier_t = ART_17::Java::brooks_read_barrier_t;

template<class T = no_brooks_read_barrier_t>
using jobject_t = ART_17::Java::jobject_t<T>;

template<class T = no_brooks_read_barrier_t>
using jarray_t = ART_17::Java::jarray_t<T>;

template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jclass_t {
  jobject_t<T>     object;

  heap_reference_t annotation_type;           // ADDED in ART 29
  heap_reference_t class_loader;
  heap_reference_t component_type;
  heap_reference_t dex_cache;
  // heap_reference_t dex_cache_strings;      // REMOVED in ART 29
  heap_reference_t iftable;
  heap_reference_t name;
  heap_reference_t super_class;
  heap_reference_t verify_error;              // Type CHANGED from Class to Object
  heap_reference_t vtable;

  uint32_t access_flags;
  uint64_t dex_cache_strings;                 // direct_methods REPLACED with dex_cache_string
  uint64_t ifields;
  uint64_t methods;                           // ADDED in ART 29
  uint64_t sfields;
  uint32_t class_flags;                       // virtual_methods REPLACED with class_flags
  uint32_t class_size;
  uint32_t clinit_thread_id;
  int32_t  dex_class_def_idx;
  int32_t  dex_type_idx;
  // uint32_t num_direct_methods;             // REMOVED in ART 29
  // uint32_t num_instance_fields;            // REMOVED in ART 29
  uint32_t num_reference_instance_fields;
  uint32_t num_reference_static_fields;
  // uint32_t num_static_fields;              // REMOVED in ART 29
  // uint32_t num_virtual_methods;            // REMOVED in ART 29
  uint32_t object_size;
  uint32_t primitive_type;
  uint32_t reference_instance_offsets;
  int32_t  status;

  uint16_t copied_methods_offset;              // ADDED in ART 29
  uint16_t virtual_methods_offset;             // ADDED in ART 29
};


// No changes in jstring structure
template<class T = no_brooks_read_barrier_t>
using jstring_t = ART_17::Java::jstring_t<T>;

template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jdex_cache_t {
  jobject_t<T> object;

  heap_reference_t dex;
  heap_reference_t location;
  uint64_t         dex_file;               // LOCATION CHANGED
  uint64_t         resolved_fields;        // TYPE CHANGED from heap_reference_t to uint64_t
  uint64_t         resolved_methods;       // TYPE CHANGED from heap_reference_t to uint64_t
  uint64_t         resolved_types;         // TYPE CHANGED from heap_reference_t to uint64_t
  uint64_t         strings;                // TYPE CHANGED from heap_reference_t to uint64_t
  uint32_t         num_resolved_fields;    // ADDED in ART 29
  uint32_t         num_resolved_methods;   // ADDED in ART 29
  uint32_t         num_resolved_types;     // ADDED in ART 29
  uint32_t         num_strings;            // ADDED in ART 29
};




} // Namespace Java
} // Namespace ART_29


// ======================
// Android 7.1.X - ART 30
// ======================
namespace ART_30 {

/// Namespace related to the Java part of ART 30
namespace Java {

using heap_reference_t      = ART_29::Java::heap_reference_t;
using brooks_read_barrier_t = ART_29::Java::brooks_read_barrier_t;

template<class T = no_brooks_read_barrier_t>
using jobject_t = ART_29::Java::jobject_t<T>;

template<class T = no_brooks_read_barrier_t>
using jarray_t = ART_29::Java::jarray_t<T>;

template<class T = no_brooks_read_barrier_t>
using jclass_t = ART_29::Java::jclass_t<T>;

// No changes in jstring structure
template<class T = no_brooks_read_barrier_t>
using jstring_t = ART_29::Java::jstring_t<T>;

// No changes in jdex_cache structure
template<class T = no_brooks_read_barrier_t>
using jdex_cache_t = ART_29::Java::jdex_cache_t<T>;

} // Namespace Java
} // Namespace ART_30

// ======================
// Android 8.0.0 - ART 44
// ======================
namespace ART_44 {

/// Namespace related to the Java part of ART 44
namespace Java {


using heap_reference_t      = ART_30::Java::heap_reference_t;
using brooks_read_barrier_t = ART_30::Java::brooks_read_barrier_t;

template<class T = no_brooks_read_barrier_t>
using jobject_t = ART_30::Java::jobject_t<T>;

template<class T = no_brooks_read_barrier_t>
using jarray_t = ART_30::Java::jarray_t<T>;

template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jclass_t {
  jobject_t<T>     object;

  // heap_reference_t annotation_type;        // REMOVED in ART 44
  heap_reference_t class_loader;
  heap_reference_t component_type;
  heap_reference_t dex_cache;
  heap_reference_t ext_data;                  // ADDED in ART 44
  heap_reference_t iftable;
  heap_reference_t name;
  heap_reference_t super_class;
  // heap_reference_t verify_error;           // REMOVED in ART 44
  heap_reference_t vtable;

  // uint32_t access_flags;                   // REMOVED in ART 44
  // uint64_t dex_cache_strings;              // REMOVED in ART 44
  uint64_t ifields;
  uint64_t methods;
  uint64_t sfields;
  uint32_t access_flags;                      // ADDED in ART 44
  uint32_t class_flags;
  uint32_t class_size;
  uint32_t clinit_thread_id;
  int32_t  dex_class_def_idx;
  int32_t  dex_type_idx;
  uint32_t num_reference_instance_fields;
  uint32_t num_reference_static_fields;
  uint32_t object_size;
  uint32_t object_size_alloc_fast_path;       // ADDED in ART 44
  uint32_t primitive_type;
  uint32_t reference_instance_offsets;
  int32_t  status;
  uint16_t copied_methods_offset;
  uint16_t virtual_methods_offset;
};


// No changes in jstring structure but string can be
// encoded as as char16_t or char (compressed)
// count[0] (LSB) == 1 ----> compressed
// count[0] (LSB) == 0 ----> chat16_t
template<class T = no_brooks_read_barrier_t>
using jstring_t = ART_30::Java::jstring_t<T>;

template<class T = no_brooks_read_barrier_t>
struct ALIGNED_(4) jdex_cache_t {
  jobject_t<T> object;

  // heap_reference_t dex;                     // REMOVED in ART 44
  heap_reference_t location;
  uint32_t         num_resolved_call_sites;    // ADDED in ART 44 (related to DEX38 format)
  uint64_t         dex_file;
  uint64_t         resolved_call_sites;        // ADDED in ART 44 (related to DEX38 format)
  uint64_t         resolved_fields;
  uint64_t         resolved_method_types;      // ADDED in ART 44
  uint64_t         resolved_methods;
  uint64_t         resolved_types;
  uint64_t         strings;
  uint32_t         num_resolved_fields;
  uint32_t         num_resolved_methods_types; // ADDED in ART 44
  uint32_t         num_resolved_methods;
  uint32_t         num_resolved_types;
  uint32_t         num_strings;
};


} // Namespace Java
} // Namespace ART_44


// ======================
// Android 8.1.X - ART 46
// ======================
namespace ART_46 {

/// Namespace related to the Java part of ART 46
namespace Java {

using heap_reference_t      = ART_44::Java::heap_reference_t;
using brooks_read_barrier_t = ART_44::Java::brooks_read_barrier_t;

template<class T = no_brooks_read_barrier_t>
using jobject_t = ART_44::Java::jobject_t<T>;

template<class T = no_brooks_read_barrier_t>
using jarray_t = ART_44::Java::jarray_t<T>;

template<class T = no_brooks_read_barrier_t>
using jclass_t = ART_44::Java::jclass_t<T>;

template<class T = no_brooks_read_barrier_t>
using jstring_t = ART_44::Java::jstring_t<T>;

template<class T = no_brooks_read_barrier_t>
using jdex_cache_t = ART_44::Java::jdex_cache_t<T>;

} // Namespace Java
} // Namespace ART_46

// ======================
// Android 9.0.0 - ART 66
// ======================
namespace ART_56 {

/// Namespace related to the Java part of ART 46
namespace Java {

using heap_reference_t      = ART_46::Java::heap_reference_t;
using brooks_read_barrier_t = ART_46::Java::brooks_read_barrier_t;

template<class T = no_brooks_read_barrier_t>
using jobject_t = ART_46::Java::jobject_t<T>;

template<class T = no_brooks_read_barrier_t>
using jarray_t = ART_46::Java::jarray_t<T>;

template<class T = no_brooks_read_barrier_t>
using jclass_t = ART_46::Java::jclass_t<T>;

template<class T = no_brooks_read_barrier_t>
using jstring_t = ART_46::Java::jstring_t<T>;

template<class T = no_brooks_read_barrier_t>
using jdex_cache_t = ART_46::Java::jdex_cache_t<T>;

} // Namespace Java
} // Namespace ART_56

} // namespace details
} // Namespace ART
} // Namespace LIEF



#endif
