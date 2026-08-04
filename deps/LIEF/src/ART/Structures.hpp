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
#ifndef LIEF_ART_STRUCTURES_H
#define LIEF_ART_STRUCTURES_H

#include "LIEF/types.hpp"
#include "LIEF/ART/enums.hpp"
#include "LIEF/ART/types.hpp"
#include "LIEF/ART/java_structures.hpp"


// ======================
// Android 6.0.0: ART 17
// Android 6.0.1: ART 17
//
// Android 7.0.0: ART 29
//
// Android 7.1.0: ART 30
// Android 7.1.1: ART 30
// Android 7.1.2: ART 30
//
// Android 8.0.0: ART 44
// Android 8.1.0: ART 46
//
// Android 9.0.0 ART 56
// ======================

namespace LIEF {
/// Namespace related to the LIEF's ART module
namespace ART {

namespace details {

static constexpr uint8_t art_magic[]       = { 'a', 'r', 't', '\n' };
static constexpr art_version_t art_version = 0;

struct ALIGNED_(4) image_section_t {
  uint32_t offset;
  uint32_t size;
};


// ==============================
// ART 17
//   - Android 6.0.0
//   - Android 6.0.1
// ==============================
namespace ART_17 {
static constexpr art_version_t art_version = 17;

static constexpr uint32_t nb_sections      = 5;
static constexpr uint32_t nb_image_methods = 6;
static constexpr uint32_t nb_image_roots   = 2;

struct ALIGNED_(4) header {
  uint8_t magic[4];
  uint8_t version[4];

  // Required base address for mapping the image.
  uint32_t image_begin;

  // Image size, not page aligned.
  uint32_t image_size;

  // Checksum of the oat file we link to for load time sanity check.
  uint32_t oat_checksum;

  // Start address for oat file. Will be before oat_data_begin_ for .so files.
  uint32_t oat_file_begin;

  // Required oat address expected by image Method::GetCode() pointers.
  uint32_t oat_data_begin;

  // End of oat data address range for this image file.
  uint32_t oat_data_end;

  // End of oat file address range. will be after oat_data_end_ for
  // .so files. Used for positioning a following alloc spaces.
  uint32_t oat_file_end;

  // The total delta that this image has been patched.
  int32_t patch_delta;

  // Absolute address of an Object[] of objects needed to reinitialize from an image.
  uint32_t image_roots;

  // Pointer size, this affects the size of the ArtMethods.
  uint32_t pointer_size;

  // Boolean (0 or 1) to denote if the image was compiled with --compile-pic option
  uint32_t compile_pic;

  // Image sections
  image_section_t sections[nb_sections];

  // Image methods.
  uint64_t image_methods[nb_image_methods];

};


} // Namespace ART_17

// ==============================
// ART 29
//   - Android 7.0.0
// ==============================
namespace ART_29 {

static constexpr art_version_t art_version = 29;

static constexpr uint32_t nb_sections      = 9;
static constexpr uint32_t nb_image_methods = 6;
static constexpr uint32_t nb_image_roots   = 2;

struct ALIGNED_(4) header {

  uint8_t magic[4];
  uint8_t version[4];

  // Required base address for mapping the image.
  uint32_t image_begin;

  // Image size, not page aligned.
  uint32_t image_size;

  // Checksum of the oat file we link to for load time sanity check.
  uint32_t oat_checksum;

  // Start address for oat file. Will be before oat_data_begin_ for .so files.
  uint32_t oat_file_begin;

  // Required oat address expected by image Method::GetCode() pointers.
  uint32_t oat_data_begin;

  // End of oat data address range for this image file.
  uint32_t oat_data_end;

  // End of oat file address range. will be after oat_data_end_ for
  // .so files. Used for positioning a following alloc spaces.
  uint32_t oat_file_end;

  // Boot image begin and end (app image headers only).
  uint32_t boot_image_begin;
  uint32_t boot_image_size;

  // Boot oat begin and end (app image headers only).
  uint32_t boot_oat_begin;
  uint32_t boot_oat_size;

  // The total delta that this image has been patched.
  int32_t patch_delta;

  // Absolute address of an Object[] of objects needed to reinitialize from an image.
  uint32_t image_roots;

  // Pointer size, this affects the size of the ArtMethods.
  uint32_t pointer_size;

  // Boolean (0 or 1) to denote if the image was compiled with --compile-pic option
  uint32_t compile_pic;

  // Boolean (0 or 1) to denote if the image can be mapped at a random address, this only refers to
  // the .art file. Currently, app oat files do not depend on their app image. There are no pointers
  // from the app oat code to the app image.
  uint32_t is_pic;

  // Image sections
  image_section_t sections[nb_sections];

  // Image methods.
  uint64_t image_methods[nb_image_methods];

  // Storage method for the image, the image may be compressed.
  uint32_t storage_mode;

  // Data size for the image data excluding the bitmap and the header. For compressed images, this
  // is the compressed size in the file.
  uint32_t data_size;
};

} // Namespace ART_29


// ==============================
// ART 30
//   - Android 7.1.0
//   - Android 7.1.1
//   - Android 7.1.2
// ==============================
namespace ART_30 {

static constexpr art_version_t art_version = 30;

static constexpr uint32_t nb_sections      = 10;
static constexpr uint32_t nb_image_methods = 6;
static constexpr uint32_t nb_image_roots   = 2;


struct ALIGNED_(4) header {

  uint8_t magic[4];
  uint8_t version[4];

  // Required base address for mapping the image.
  uint32_t image_begin;

  // Image size, not page aligned.
  uint32_t image_size;

  // Checksum of the oat file we link to for load time sanity check.
  uint32_t oat_checksum;

  // Start address for oat file. Will be before oat_data_begin_ for .so files.
  uint32_t oat_file_begin;

  // Required oat address expected by image Method::GetCode() pointers.
  uint32_t oat_data_begin;

  // End of oat data address range for this image file.
  uint32_t oat_data_end;

  // End of oat file address range. will be after oat_data_end_ for
  // .so files. Used for positioning a following alloc spaces.
  uint32_t oat_file_end;

  // Boot image begin and end (app image headers only).
  uint32_t boot_image_begin;
  uint32_t boot_image_size;

  // Boot oat begin and end (app image headers only).
  uint32_t boot_oat_begin;
  uint32_t boot_oat_size;

  // The total delta that this image has been patched.
  int32_t patch_delta;

  // Absolute address of an Object[] of objects needed to reinitialize from an image.
  uint32_t image_roots;

  // Pointer size, this affects the size of the ArtMethods.
  uint32_t pointer_size;

  // Boolean (0 or 1) to denote if the image was compiled with --compile-pic option
  uint32_t compile_pic;

  // Boolean (0 or 1) to denote if the image can be mapped at a random address, this only refers to
  // the .art file. Currently, app oat files do not depend on their app image. There are no pointers
  // from the app oat code to the app image.
  uint32_t is_pic;

  // Image sections
  image_section_t sections[nb_sections];

  // Image methods.
  uint64_t image_methods[nb_image_methods];

  // Storage method for the image, the image may be compressed.
  uint32_t storage_mode;

  // Data size for the image data excluding the bitmap and the header. For compressed images, this
  // is the compressed size in the file.
  uint32_t data_size;
};

} // Namespace ART_30


// ==============================
// ART 44
//   - Android 8.0.0
// ==============================
namespace ART_44 {

static constexpr art_version_t art_version = 44;

static constexpr uint32_t nb_sections      = 10;
static constexpr uint32_t nb_image_methods = 7;
static constexpr uint32_t nb_image_roots   = 3;


struct ALIGNED_(4) header {

  uint8_t magic[4];
  uint8_t version[4];

  // Required base address for mapping the image.
  uint32_t image_begin;

  // Image size, not page aligned.
  uint32_t image_size;

  // Checksum of the oat file we link to for load time sanity check.
  uint32_t oat_checksum;

  // Start address for oat file. Will be before oat_data_begin_ for .so files.
  uint32_t oat_file_begin;

  // Required oat address expected by image Method::GetCode() pointers.
  uint32_t oat_data_begin;

  // End of oat data address range for this image file.
  uint32_t oat_data_end;

  // End of oat file address range. will be after oat_data_end_ for
  // .so files. Used for positioning a following alloc spaces.
  uint32_t oat_file_end;

  // Boot image begin and end (app image headers only).
  uint32_t boot_image_begin;
  uint32_t boot_image_size;

  // Boot oat begin and end (app image headers only).
  uint32_t boot_oat_begin;
  uint32_t boot_oat_size;

  // The total delta that this image has been patched.
  int32_t patch_delta;

  // Absolute address of an Object[] of objects needed to reinitialize from an image.
  uint32_t image_roots;

  // Pointer size, this affects the size of the ArtMethods.
  uint32_t pointer_size;

  // Boolean (0 or 1) to denote if the image was compiled with --compile-pic option
  uint32_t compile_pic;

  // Boolean (0 or 1) to denote if the image can be mapped at a random address, this only refers to
  // the .art file. Currently, app oat files do not depend on their app image. There are no pointers
  // from the app oat code to the app image.
  uint32_t is_pic;

  // Image sections
  image_section_t sections[nb_sections];

  // Image methods.
  uint64_t image_methods[nb_image_methods];

  // Storage method for the image, the image may be compressed.
  uint32_t storage_mode;

  // Data size for the image data excluding the bitmap and the header. For compressed images, this
  // is the compressed size in the file.
  uint32_t data_size;
};


} // Namespace ART_44


// ==============================
// ART 46
//   - Android 8.1.0
// ==============================
namespace ART_46 {

static constexpr art_version_t art_version = 46;
static constexpr uint32_t nb_image_roots   = 3;

// No changes in the structure
using header = ART_44::header;

} // Namespace ART_46

// ==============================
// ART 56
//   - Android 9.0.0
// ==============================
namespace ART_56 {

static constexpr art_version_t art_version = 56;
static constexpr uint32_t nb_sections      = 10;
static constexpr uint32_t nb_image_roots   = 3;
static constexpr uint32_t nb_image_methods = 9; // Android 9.0.0 - +=2
struct ALIGNED_(4) header {

  uint8_t magic[4];
  uint8_t version[4];

  // Required base address for mapping the image.
  uint32_t image_begin;

  // Image size, not page aligned.
  uint32_t image_size;

  // Checksum of the oat file we link to for load time sanity check.
  uint32_t oat_checksum;

  // Start address for oat file. Will be before oat_data_begin_ for .so files.
  uint32_t oat_file_begin;

  // Required oat address expected by image Method::GetCode() pointers.
  uint32_t oat_data_begin;

  // End of oat data address range for this image file.
  uint32_t oat_data_end;

  // End of oat file address range. will be after oat_data_end_ for
  // .so files. Used for positioning a following alloc spaces.
  uint32_t oat_file_end;

  // Boot image begin and end (app image headers only).
  uint32_t boot_image_begin;
  uint32_t boot_image_size;

  // Boot oat begin and end (app image headers only).
  uint32_t boot_oat_begin;
  uint32_t boot_oat_size;

  // The total delta that this image has been patched.
  int32_t patch_delta;

  // Absolute address of an Object[] of objects needed to reinitialize from an image.
  uint32_t image_roots;

  // Pointer size, this affects the size of the ArtMethods.
  uint32_t pointer_size;

  // Boolean (0 or 1) to denote if the image was compiled with --compile-pic option
  uint32_t compile_pic;

  // Boolean (0 or 1) to denote if the image can be mapped at a random address, this only refers to
  // the .art file. Currently, app oat files do not depend on their app image. There are no pointers
  // from the app oat code to the app image.
  uint32_t is_pic;

  // Image sections
  image_section_t sections[nb_sections];

  // Image methods.
  uint64_t image_methods[nb_image_methods];

  // Storage method for the image, the image may be compressed.
  uint32_t storage_mode;

  // Data size for the image data excluding the bitmap and the header. For compressed images, this
  // is the compressed size in the file.
  uint32_t data_size;
};


} // Namespace ART_56


class ART17 {
  public:
  using art_header_t                         = ART_17::header;
  static constexpr art_version_t art_version = ART_17::art_version;
  static constexpr uint32_t nb_image_roots   = ART_17::nb_image_roots;

  using IMAGE_SECTIONS                       = ART::ART_17::IMAGE_SECTIONS;
  using IMAGE_METHODS                        = ART::ART_17::IMAGE_METHODS;
  using IMAGE_ROOTS                          = ART::ART_17::IMAGE_ROOTS;

  // =====================
  // Java
  // =====================
  template<class T = no_brooks_read_barrier_t>
  using jobject_t = ART_17::Java::jobject_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jarray_t = ART_17::Java::jarray_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jclass_t = ART_17::Java::jclass_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jstring_t = ART_17::Java::jstring_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jdex_cache_t = ART_17::Java::jdex_cache_t<T>;
};

class ART29 {
  public:
  using art_header_t                         = ART_29::header;
  static constexpr art_version_t art_version = ART_29::art_version;
  static constexpr uint32_t nb_image_roots   = ART_29::nb_image_roots;

  using IMAGE_SECTIONS                       = ART::ART_29::IMAGE_SECTIONS;
  using IMAGE_METHODS                        = ART::ART_29::IMAGE_METHODS;
  using IMAGE_ROOTS                          = ART::ART_29::IMAGE_ROOTS;

  // =====================
  // Java
  // =====================
  template<class T = no_brooks_read_barrier_t>
  using jobject_t = ART_17::Java::jobject_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jarray_t = ART_29::Java::jarray_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jclass_t = ART_29::Java::jclass_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jstring_t = ART_29::Java::jstring_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jdex_cache_t = ART_29::Java::jdex_cache_t<T>;
};

class ART30 {
  public:
  using art_header_t                         = ART_30::header;
  static constexpr art_version_t art_version = ART_30::art_version;
  static constexpr uint32_t nb_image_roots   = ART_30::nb_image_roots;

  using IMAGE_SECTIONS                       = ART::ART_30::IMAGE_SECTIONS;
  using IMAGE_METHODS                        = ART::ART_30::IMAGE_METHODS;
  using IMAGE_ROOTS                          = ART::ART_30::IMAGE_ROOTS;

  // =====================
  // Java
  // =====================
  template<class T = no_brooks_read_barrier_t>
  using jobject_t = ART_30::Java::jobject_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jarray_t = ART_30::Java::jarray_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jclass_t = ART_30::Java::jclass_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jstring_t = ART_30::Java::jstring_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jdex_cache_t = ART_30::Java::jdex_cache_t<T>;
};

class ART44 {
  public:
  using art_header_t                         = ART_44::header;
  static constexpr art_version_t art_version = ART_44::art_version;
  static constexpr uint32_t nb_image_roots   = ART_44::nb_image_roots;

  using IMAGE_SECTIONS                       = ART::ART_44::IMAGE_SECTIONS;
  using IMAGE_METHODS                        = ART::ART_44::IMAGE_METHODS;
  using IMAGE_ROOTS                          = ART::ART_44::IMAGE_ROOTS;

  // =====================
  // Java
  // =====================
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
};

class ART46 {
  public:
  using art_header_t                         = ART_46::header;
  static constexpr art_version_t art_version = ART_46::art_version;
  static constexpr uint32_t nb_image_roots   = ART_46::nb_image_roots;

  using IMAGE_SECTIONS                       = ART::ART_46::IMAGE_SECTIONS;
  using IMAGE_METHODS                        = ART::ART_46::IMAGE_METHODS;
  using IMAGE_ROOTS                          = ART::ART_46::IMAGE_ROOTS;

  // =====================
  // Java
  // =====================
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
};

class ART56 {
  public:
  using art_header_t                         = ART_56::header;
  static constexpr art_version_t art_version = ART_56::art_version;
  static constexpr uint32_t nb_image_roots   = ART_56::nb_image_roots;

  using IMAGE_SECTIONS                       = ART::ART_46::IMAGE_SECTIONS;
  using IMAGE_METHODS                        = ART::ART_46::IMAGE_METHODS;
  using IMAGE_ROOTS                          = ART::ART_46::IMAGE_ROOTS;

  // =====================
  // Java
  // =====================
  template<class T = no_brooks_read_barrier_t>
  using jobject_t = ART_56::Java::jobject_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jarray_t = ART_56::Java::jarray_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jclass_t = ART_56::Java::jclass_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jstring_t = ART_56::Java::jstring_t<T>;

  template<class T = no_brooks_read_barrier_t>
  using jdex_cache_t = ART_56::Java::jdex_cache_t<T>;
};

}
} /* end namespace ART */
} /* end namespace LIEF */
#endif

