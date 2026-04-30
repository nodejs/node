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
#include "logging.hpp"

#include "LIEF/utils.hpp"
#include "LIEF/ART/Parser.hpp"
#include "LIEF/ART/File.hpp"
#include "LIEF/ART/EnumToString.hpp"

namespace LIEF {
namespace ART {

template<typename ART_T>
void Parser::parse_file() {
  LIEF_DEBUG("Parsing ART version {}", ART_T::art_version);
  /* const size_t ptr_size = */ parse_header<ART_T>();
}

template<typename ART_T>
size_t Parser::parse_header() {
  using art_header_t = typename ART_T::art_header_t;

  const auto res_hdr = stream_->peek<art_header_t>(0);
  if (!res_hdr) {
    return 0;
  }
  const auto hdr = std::move(*res_hdr);
  imagebase_ = hdr.image_begin;

  if (hdr.pointer_size != sizeof(uint32_t) && hdr.pointer_size != sizeof(uint64_t)) {
    LIEF_WARN("ART Header pointer_size is not consistent");
    return 0;
  }
  file_->header_ = &hdr;
  return hdr.pointer_size;
}

#if 0
template<typename ART_T, typename PTR_T>
void Parser::parse_sections() {
  using IMAGE_SECTIONS = typename ART_T::IMAGE_SECTIONS;
  using art_header_t   = typename ART_T::art_header_t;
  using jarray_t       = typename ART_T::template jarray_t<>;
  using jclass_t       = typename ART_T::template jclass_t<>;
  using jobject_t      = typename ART_T::template jobject_t<>;

  VLOG(VDEBUG) << "Parsing Image Sections" << '\n';
  size_t nb_sections = file_->header().nb_sections_;

  const art_header_t& hdr = stream_->peek<art_header_t>(0);

  VLOG(VDEBUG) << "Parsing " << std::dec << file_->header().nb_sections_ << " sections";

  size_t start_offset = align(sizeof(art_header_t), sizeof(uint64_t));
  // TODO: Check section size number
  for (size_t i = 0; i < nb_sections; ++i) {
    IMAGE_SECTIONS section_type = static_cast<IMAGE_SECTIONS>(i);
    ART::image_section_t section_header = hdr.sections[i];

    uint64_t jarray_offset = start_offset;
    if (i == 1) {
      jarray_offset = align(sizeof(art_header_t) + sizeof(jarray_t) + (3409 - 1) * sizeof(uint32_t), sizeof(uint64_t));
    }

    if (i == 2) {
      jarray_offset = align(sizeof(art_header_t) + sizeof(jarray_t) + (3409 - 1) * sizeof(uint32_t), sizeof(uint64_t));
      jarray_offset += sizeof(jarray_t) + 2 * (32062 - 1) * (sizeof(uint32_t));
      jarray_offset = align(jarray_offset, sizeof(uint64_t));
      //jarray_offset = 0x700e1db0 - imagebase_;
    }
    std::cout << "addr:" << std::hex << imagebase_ + jarray_offset << '\n';

    const jarray_t* array = reinterpret_cast<const jarray_t*>(stream_->read(jarray_offset, sizeof(jarray_t)));
    uint64_t elements_offset = jarray_offset + offsetof(jarray_t, elements);



    VLOG(VDEBUG) << to_string(section_type) << "@" << std::hex << section_header.offset
                 << " --> " << (section_header.offset + section_header.size)
                 << " #" << std::dec << array->length;

    std::cout << std::hex << stream_->read_integer<uint32_t>(jarray_offset) << '\n';
    std::cout << std::hex << stream_->read_integer<uint32_t>(jarray_offset + sizeof(uint32_t)) << '\n';
    std::cout << std::hex << stream_->read_integer<uint32_t>(jarray_offset + 2 * sizeof(uint32_t)) << '\n';
    std::cout << std::hex << stream_->read_integer<uint32_t>(jarray_offset + 3 * sizeof(uint32_t)) << '\n';
    std::cout << std::hex << stream_->read_integer<uint32_t>(jarray_offset + 4 * sizeof(uint32_t)) << '\n';


    uint32_t parent  = array->object.klass - imagebase_;
    const jclass_t* pp = reinterpret_cast<const jclass_t*>(stream_->read(parent, sizeof(jclass_t)));
    parse_jstring<ART_T, PTR_T>(pp->name - imagebase_);

    switch (section_type) {
      case IMAGE_SECTIONS::SECTION_OBJECTS:
        {

          // '0x70000090'
          parse_objects<ART_T, PTR_T>(elements_offset, array->length);
          break;
        }

      case IMAGE_SECTIONS::SECTION_ART_FIELDS:
        {
          // '0x700035e0'
          parse_art_fields<ART_T, PTR_T>(elements_offset, array->length);
          break;
        }

      case IMAGE_SECTIONS::SECTION_ART_METHODS:
        {
          // 0x6ff99db0: long[] length:65533
          parse_art_methods<ART_T, PTR_T>(elements_offset, array->length);
          break;
        }

      case IMAGE_SECTIONS::SECTION_INTERNED_STRINGS:
        {
          parse_interned_strings<ART_T, PTR_T>(elements_offset, array->length);
          break;
        }

      default:
        {
          LOG(WARNING) << to_string(section_type) << " is !handle yet!";
        }
    }

  }
}


template<typename ART_T, typename PTR_T>
void Parser::parse_roots() {
  using jarray_t = typename ART_T::template jarray_t<>;
  VLOG(VDEBUG) << "Parsing Image Roots" << '\n';
  using IMAGE_ROOTS = typename ART_T::IMAGE_ROOTS;

  uint32_t image_root_offset = file_->header().image_roots_ - file_->header().image_begin_;

  VLOG(VDEBUG) << "Image root at: " << std::hex << std::showbase << file_->header().image_roots_;
  VLOG(VDEBUG) << "Image root offset: " << std::hex << std::showbase << image_root_offset;

  const jarray_t* array = reinterpret_cast<const jarray_t*>(stream_->read(image_root_offset, sizeof(jarray_t)));
  std::cout << "Nb elements: " << array->length << '\n';

  const uint32_t* array_values = reinterpret_cast<const uint32_t*>(
      stream_->read(
        image_root_offset + offsetof(jarray_t, elements),
        array->length * sizeof(uint32_t)
      ));


  for (size_t i = 0; i < ART_T::nb_image_roots; ++i) {
    IMAGE_ROOTS type = static_cast<IMAGE_ROOTS>(i);
    uint64_t struct_offset = array_values[i] - imagebase_;
    std::cout << std::hex << struct_offset << '\n';
    const jarray_t* array = reinterpret_cast<const jarray_t*>(stream_->read(struct_offset, sizeof(jarray_t)));
    std::cout << "Nb elements: " << std::dec << array->length << '\n';
    switch (type) {
      case IMAGE_ROOTS::DEX_CACHES:
        {
          parse_dex_caches<ART_T, PTR_T>(struct_offset + offsetof(jarray_t, elements), array->length);
          break;
        }

      case IMAGE_ROOTS::CLASS_ROOTS:
        {
          parse_class_roots<ART_T, PTR_T>(struct_offset + offsetof(jarray_t, elements), array->length);
          break;
        }

      case ART_44::IMAGE_ROOTS::CLASS_LOADER:
        {
          //parse_dex_caches<ART_T, PTR_T>(struct_offset + offsetof(jarray_t, elements), array->length);
          break;
        }

      default:
        {
          LOG(WARNING) << to_string(type) << " is !handle yet!";
        }
    }

  }
}


template<typename ART_T, typename PTR_T>
void Parser::parse_class_roots(size_t offset, size_t size) {
  using jclass_t = typename ART_T::template jclass_t<>;
  using jstring_t    = typename ART_T::template jstring_t<>;
  VLOG(VDEBUG) << "Parsing Class Roots" << '\n';

  for (size_t i = 0; i < size; ++i) {
    uint32_t object_offset = stream_->read_integer<uint32_t>(offset + i * sizeof(uint32_t)) - imagebase_;
    parse_class<ART_T, PTR_T>(object_offset);
  }
}

template<typename ART_T, typename PTR_T>
void Parser::parse_class(size_t offset) {
  using jclass_t     = typename ART_T::template jclass_t<>;
  using jstring_t    = typename ART_T::template jstring_t<>;

  const jclass_t* cls = reinterpret_cast<const jclass_t*>(stream_->read(offset, sizeof(jclass_t)));
  parse_jstring<ART_T, PTR_T>(cls->name - imagebase_);
}

template<typename ART_T, typename PTR_T>
void Parser::parse_jstring(size_t offset) {
  using jstring_t    = typename ART_T::template jstring_t<>;
  const jstring_t* jstring = reinterpret_cast<const jstring_t*>(stream_->read(offset, sizeof(jstring_t)));
  //std::cout << "Class leng: " << std::dec << jstring->count << '\n';

  uint64_t value_offset = offset + offsetof(jstring_t, value);

  std::u16string str = {
    reinterpret_cast<const char16_t*>(stream_->read(value_offset, static_cast<uint16_t>(jstring->count) * sizeof(char16_t))),
    static_cast<uint16_t>(jstring->count)
  };
  std::cout << u16tou8(str)  << '\n';
}

template<typename ART_T, typename PTR_T>
void Parser::parse_dex_caches(size_t offset, size_t size) {
  using jobject_t    = typename ART_T::template jobject_t<>;
  using jarray_t     = typename ART_T::template jarray_t<>;
  using jclass_t     = typename ART_T::template jclass_t<>;
  using jstring_t    = typename ART_T::template jstring_t<>;
  using jdex_cache_t = typename ART_T::template jdex_cache_t<>;

  VLOG(VDEBUG) << "Parsing Dex Cache" << '\n';

  for (size_t i = 0; i < size; ++i) {
    uint32_t object_offset = stream_->read_integer<uint32_t>(offset + i * sizeof(uint32_t)) - imagebase_;
    parse_dex_cache<ART_T, PTR_T>(object_offset);
  }
}

template<typename ART_T, typename PTR_T>
void Parser::parse_dex_cache(size_t object_offset) {
  using jstring_t    = typename ART_T::template jstring_t<>;
  using jdex_cache_t = typename ART_T::template jdex_cache_t<>;
  const jdex_cache_t* cache = reinterpret_cast<const jdex_cache_t*>(stream_->read(object_offset, sizeof(jdex_cache_t)));
  const jstring_t* location = reinterpret_cast<const jstring_t*>(stream_->read(cache->location - imagebase_, sizeof(jstring_t)));

  uint64_t name_offset = cache->location - imagebase_ + offsetof(jstring_t, value);

  if (location->count & 1) {
    size_t len = location->count >> 1;

    std::string location_string = {
      reinterpret_cast<const char*>(stream_->read(name_offset, static_cast<uint16_t>(len) * sizeof(char))),
      len
    };
    std::cout << location_string  << '\n';
  } else {

    std::u16string location_string = {
      reinterpret_cast<const char16_t*>(stream_->read(name_offset, static_cast<uint16_t>(location->count) * sizeof(char16_t))),
      static_cast<uint16_t>(location->count)
    };
    std::cout << u16tou8(location_string)  << '\n';
  }
}

template<typename ART_T, typename PTR_T>
void Parser::parse_methods() {
  using art_header_t = typename ART_T::art_header_t;
  using IMAGE_METHODS = typename ART_T::IMAGE_METHODS;

  VLOG(VDEBUG) << "Parsing Image Methods" << '\n';


  const art_header_t* hdr = reinterpret_cast<const art_header_t*>(stream_->read(0, sizeof(art_header_t)));

  uint32_t nb_methods = file_->header().nb_methods_;
  //TODO check with ART::nb_methods... (more secure)
  for (size_t i = 0; i < nb_methods; ++i) {
    IMAGE_METHODS type = static_cast<IMAGE_METHODS>(i);
    uint64_t address = hdr->image_methods[i];
    VLOG(VDEBUG) << to_string(type) << " at " << std::showbase << std::hex << address;
  }
}


template<typename ART_T, typename PTR_T>
void Parser::parse_objects(size_t offset, size_t size) {
  using jobject_t = typename ART_T::template jobject_t<>;
  using jarray_t  = typename ART_T::template jarray_t<>;
  using jclass_t  = typename ART_T::template jclass_t<>;
  using jstring_t = typename ART_T::template jstring_t<>;

  VLOG(VDEBUG) << "Paring objects at " << std::hex << offset << '\n';
  //const jarray_t* array = reinterpret_cast<const jarray_t*>(stream_->read(offset, sizeof(jarray_t)));
  //std::cout << std::dec << "nb elements " << array->length << '\n';;
}


template<typename ART_T, typename PTR_T>
void Parser::parse_art_fields(size_t offset, size_t size) {
  VLOG(VDEBUG) << "Paring ART Fields at " << std::hex << offset << '\n';
}

template<typename ART_T, typename PTR_T>
void Parser::parse_art_methods(size_t offset, size_t size) {
  VLOG(VDEBUG) << "Paring ART Methods at " << std::hex << offset << '\n';
  // 0x6ff99db0: long[] length:65533
  const PTR_T* methods = reinterpret_cast<const PTR_T*>(stream_->read(offset, size));
  // 26740: 0x70a7ea60
  PTR_T get_device_id = methods[26740];

  std::cout << std::hex << "Get device ID " << get_device_id << '\n';
}

template<typename ART_T, typename PTR_T>
void Parser::parse_interned_strings(size_t offset, size_t size) {

}
#endif

}
}
