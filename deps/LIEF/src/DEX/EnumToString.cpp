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
#include "LIEF/DEX/EnumToString.hpp"
#include "frozen.hpp"

namespace LIEF {
namespace DEX {

const char* to_string(MapItem::TYPES  e) {
  CONST_MAP(MapItem::TYPES, const char*, 20) enumStrings {
    { MapItem::TYPES::HEADER,                   "HEADER" },
    { MapItem::TYPES::STRING_ID,                "STRING_ID" },
    { MapItem::TYPES::TYPE_ID,                  "TYPE_ID" },
    { MapItem::TYPES::PROTO_ID,                 "PROTO_ID" },
    { MapItem::TYPES::FIELD_ID,                 "FIELD_ID" },
    { MapItem::TYPES::METHOD_ID,                "METHOD_ID" },
    { MapItem::TYPES::CLASS_DEF,                "CLASS_DEF" },
    { MapItem::TYPES::CALL_SITE_ID,             "CALL_SITE_ID" },
    { MapItem::TYPES::METHOD_HANDLE,            "METHOD_HANDLE" },
    { MapItem::TYPES::MAP_LIST,                 "MAP_LIST" },
    { MapItem::TYPES::TYPE_LIST,                "TYPE_LIST" },
    { MapItem::TYPES::ANNOTATION_SET_REF_LIST,  "ANNOTATION_SET_REF_LIST" },
    { MapItem::TYPES::ANNOTATION_SET,           "ANNOTATION_SET" },
    { MapItem::TYPES::CLASS_DATA,               "CLASS_DATA" },
    { MapItem::TYPES::CODE,                     "CODE" },
    { MapItem::TYPES::STRING_DATA,              "STRING_DATA" },
    { MapItem::TYPES::DEBUG_INFO,               "DEBUG_INFO" },
    { MapItem::TYPES::ANNOTATION,               "ANNOTATION" },
    { MapItem::TYPES::ENCODED_ARRAY,            "ENCODED_ARRAY" },
    { MapItem::TYPES::ANNOTATIONS_DIRECTORY,    "ANNOTATIONS_DIRECTORY" },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}


const char* to_string(ACCESS_FLAGS  e) {
  CONST_MAP(ACCESS_FLAGS, const char*, 18) enumStrings {
    { ACCESS_FLAGS::ACC_UNKNOWN,               "UNKNOWN" },
    { ACCESS_FLAGS::ACC_PUBLIC,                "PUBLIC" },
    { ACCESS_FLAGS::ACC_PRIVATE,               "PRIVATE" },
    { ACCESS_FLAGS::ACC_PROTECTED,             "PROTECTED" },
    { ACCESS_FLAGS::ACC_STATIC,                "STATIC" },
    { ACCESS_FLAGS::ACC_FINAL,                 "FINAL" },
    { ACCESS_FLAGS::ACC_SYNCHRONIZED,          "SYNCHRONIZED" },
    { ACCESS_FLAGS::ACC_VOLATILE,              "VOLATILE" },
    { ACCESS_FLAGS::ACC_VARARGS,               "VARARGS" },
    { ACCESS_FLAGS::ACC_NATIVE,                "NATIVE" },
    { ACCESS_FLAGS::ACC_INTERFACE,             "INTERFACE" },
    { ACCESS_FLAGS::ACC_ABSTRACT,              "ABSTRACT" },
    { ACCESS_FLAGS::ACC_STRICT,                "STRICT" },
    { ACCESS_FLAGS::ACC_SYNTHETIC,             "SYNTHETIC" },
    { ACCESS_FLAGS::ACC_ANNOTATION,            "ANNOTATION" },
    { ACCESS_FLAGS::ACC_ENUM,                  "ENUM" },
    { ACCESS_FLAGS::ACC_CONSTRUCTOR,           "CONSTRUCTOR" },
    { ACCESS_FLAGS::ACC_DECLARED_SYNCHRONIZED, "DECLARED_SYNCHRONIZED" },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}

const char* to_string(Type::TYPES  e) {
  CONST_MAP(Type::TYPES, const char*, 4) enumStrings {
    { Type::TYPES::UNKNOWN,   "UNKNOWN"   },
    { Type::TYPES::ARRAY,     "ARRAY"     },
    { Type::TYPES::CLASS,     "CLASS"     },
    { Type::TYPES::PRIMITIVE, "PRIMITIVE" },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}


const char* to_string(Type::PRIMITIVES  e) {
  CONST_MAP(Type::PRIMITIVES, const char*, 9) enumStrings {
    { Type::PRIMITIVES::VOID_T,  "VOID_T"    },
    { Type::PRIMITIVES::BOOLEAN, "BOOLEAN" },
    { Type::PRIMITIVES::INT,     "INT"     },
    { Type::PRIMITIVES::SHORT,   "SHORT"   },
    { Type::PRIMITIVES::LONG,    "LONG"    },
    { Type::PRIMITIVES::CHAR,    "CHAR"    },
    { Type::PRIMITIVES::DOUBLE,  "DOUBLE"  },
    { Type::PRIMITIVES::FLOAT,   "FLOAT"   },
    { Type::PRIMITIVES::BYTE,    "BYTE"    },
  };
  const auto it = enumStrings.find(e);
  return it == enumStrings.end() ? "UNKNOWN" : it->second;
}

} // namespace DEX
} // namespace LIEF
