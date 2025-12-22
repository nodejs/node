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

#include "LIEF/DEX/instructions.hpp"

#include <algorithm>
#include <map>

namespace LIEF {
namespace DEX {

INST_FORMATS inst_format_from_opcode(OPCODES op) {
  static const std::map<OPCODES, INST_FORMATS> size_map {
    { OPCODES::OP_NOP,                    INST_FORMATS::F_10x },
    { OPCODES::OP_MOVE,                   INST_FORMATS::F_12x },
    { OPCODES::OP_MOVE_FROM_16,           INST_FORMATS::F_22x },
    { OPCODES::OP_MOVE_16,                INST_FORMATS::F_32x },
    { OPCODES::OP_MOVE_WIDE,              INST_FORMATS::F_12x },
    { OPCODES::OP_MOVE_WIDE_FROM_16,      INST_FORMATS::F_22x },
    { OPCODES::OP_MOVE_WIDE_16,           INST_FORMATS::F_32x },
    { OPCODES::OP_MOVE_OBJECT,            INST_FORMATS::F_12x },
    { OPCODES::OP_MOVE_OBJECT_FROM_16,    INST_FORMATS::F_22x },
    { OPCODES::OP_MOVE_OBJECT_16,         INST_FORMATS::F_32x },
    { OPCODES::OP_MOVE_RESULT,            INST_FORMATS::F_11x },
    { OPCODES::OP_MOVE_RESULT_WIDE,       INST_FORMATS::F_11x },
    { OPCODES::OP_MOVE_RESULT_OBJECT,     INST_FORMATS::F_11x },
    { OPCODES::OP_MOVE_EXCEPTION,         INST_FORMATS::F_11x },
    { OPCODES::OP_RETURN_VOID,            INST_FORMATS::F_10x },
    { OPCODES::OP_RETURN,                 INST_FORMATS::F_11x },
    { OPCODES::OP_RETURN_WIDE,            INST_FORMATS::F_11x },
    { OPCODES::OP_RETURN_OBJECT,          INST_FORMATS::F_11x },
    { OPCODES::OP_CONST_4,                INST_FORMATS::F_11n },
    { OPCODES::OP_CONST_16,               INST_FORMATS::F_21s },
    { OPCODES::OP_CONST,                  INST_FORMATS::F_31i },
    { OPCODES::OP_CONST_HIGH_16,          INST_FORMATS::F_21h },
    { OPCODES::OP_CONST_WIDE_16,          INST_FORMATS::F_21s },
    { OPCODES::OP_CONST_WIDE_32,          INST_FORMATS::F_31i },
    { OPCODES::OP_CONST_WIDE,             INST_FORMATS::F_51l },
    { OPCODES::OP_CONST_WIDE_HIGH_16,     INST_FORMATS::F_21h },
    { OPCODES::OP_CONST_STRING,           INST_FORMATS::F_21c },
    { OPCODES::OP_CONST_STRING_JUMBO,     INST_FORMATS::F_31c },
    { OPCODES::OP_CONST_CLASS,            INST_FORMATS::F_21c },
    { OPCODES::OP_MONITOR_ENTER,          INST_FORMATS::F_11x },
    { OPCODES::OP_MONITOR_EXIT,           INST_FORMATS::F_11x },
    { OPCODES::OP_CHECK_CAST,             INST_FORMATS::F_21c },
    { OPCODES::OP_INSTANCE_OF,            INST_FORMATS::F_22c },
    { OPCODES::OP_ARRAY_LENGTH,           INST_FORMATS::F_12x },
    { OPCODES::OP_NEW_INSTANCE,           INST_FORMATS::F_21c },
    { OPCODES::OP_NEW_ARRAY,              INST_FORMATS::F_22c },
    { OPCODES::OP_FILLED_NEW_ARRAY,       INST_FORMATS::F_35c },
    { OPCODES::OP_FILLED_NEW_ARRAY_RANGE, INST_FORMATS::F_3rc },
    { OPCODES::OP_FILL_ARRAY_DATA,        INST_FORMATS::F_31t },
    { OPCODES::OP_THROW,                  INST_FORMATS::F_11x },
    { OPCODES::OP_GOTO,                   INST_FORMATS::F_10t },
    { OPCODES::OP_GOTO_16,                INST_FORMATS::F_20t },
    { OPCODES::OP_GOTO_32,                INST_FORMATS::F_30t },
    { OPCODES::OP_PACKED_SWITCH,          INST_FORMATS::F_31t },
    { OPCODES::OP_SPARSE_SWITCH,          INST_FORMATS::F_31t },
    { OPCODES::OP_CMPL_FLOAT,             INST_FORMATS::F_23x },
    { OPCODES::OP_CMPG_FLOAT,             INST_FORMATS::F_23x },
    { OPCODES::OP_CMPL_DOUBLE,            INST_FORMATS::F_23x },
    { OPCODES::OP_CMPG_DOUBLE,            INST_FORMATS::F_23x },
    { OPCODES::OP_CMP_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_IF_EQ,                  INST_FORMATS::F_22t },
    { OPCODES::OP_IF_NE,                  INST_FORMATS::F_22t },
    { OPCODES::OP_IF_LT,                  INST_FORMATS::F_22t },
    { OPCODES::OP_IF_GE,                  INST_FORMATS::F_22t },
    { OPCODES::OP_IF_GT,                  INST_FORMATS::F_22t },
    { OPCODES::OP_IF_LE,                  INST_FORMATS::F_22t },
    { OPCODES::OP_IF_EQZ,                 INST_FORMATS::F_21t },
    { OPCODES::OP_IF_NEZ,                 INST_FORMATS::F_21t },
    { OPCODES::OP_IF_LTZ,                 INST_FORMATS::F_21t },
    { OPCODES::OP_IF_GEZ,                 INST_FORMATS::F_21t },
    { OPCODES::OP_IF_GTZ,                 INST_FORMATS::F_21t },
    { OPCODES::OP_IF_LEZ,                 INST_FORMATS::F_21t },
    { OPCODES::OP_AGET,                   INST_FORMATS::F_23x },
    { OPCODES::OP_AGET_WIDE,              INST_FORMATS::F_23x },
    { OPCODES::OP_AGET_OBJECT,            INST_FORMATS::F_23x },
    { OPCODES::OP_AGET_BOOLEAN,           INST_FORMATS::F_23x },
    { OPCODES::OP_AGET_BYTE,              INST_FORMATS::F_23x },
    { OPCODES::OP_AGET_CHAR,              INST_FORMATS::F_23x },
    { OPCODES::OP_AGET_SHORT,             INST_FORMATS::F_23x },
    { OPCODES::OP_APUT,                   INST_FORMATS::F_23x },
    { OPCODES::OP_APUT_WIDE,              INST_FORMATS::F_23x },
    { OPCODES::OP_APUT_OBJECT,            INST_FORMATS::F_23x },
    { OPCODES::OP_APUT_BOOLEAN,           INST_FORMATS::F_23x },
    { OPCODES::OP_APUT_BYTE,              INST_FORMATS::F_23x },
    { OPCODES::OP_APUT_CHAR,              INST_FORMATS::F_23x },
    { OPCODES::OP_APUT_SHORT,             INST_FORMATS::F_23x },
    { OPCODES::OP_IGET,                   INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_WIDE,              INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_OBJECT,            INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_BOOLEAN,           INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_BYTE,              INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_CHAR,              INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_SHORT,             INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT,                   INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_WIDE,              INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_OBJECT,            INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_BOOLEAN,           INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_BYTE,              INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_CHAR,              INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_SHORT,             INST_FORMATS::F_22c },
    { OPCODES::OP_SGET,                   INST_FORMATS::F_21c },
    { OPCODES::OP_SGET_WIDE,              INST_FORMATS::F_21c },
    { OPCODES::OP_SGET_OBJECT,            INST_FORMATS::F_21c },
    { OPCODES::OP_SGET_BOOLEAN,           INST_FORMATS::F_21c },
    { OPCODES::OP_SGET_BYTE,              INST_FORMATS::F_21c },
    { OPCODES::OP_SGET_CHAR,              INST_FORMATS::F_21c },
    { OPCODES::OP_SGET_SHORT,             INST_FORMATS::F_21c },
    { OPCODES::OP_SPUT,                   INST_FORMATS::F_21c },
    { OPCODES::OP_SPUT_WIDE,              INST_FORMATS::F_21c },
    { OPCODES::OP_SPUT_OBJECT,            INST_FORMATS::F_21c },
    { OPCODES::OP_SPUT_BOOLEAN,           INST_FORMATS::F_21c },
    { OPCODES::OP_SPUT_BYTE,              INST_FORMATS::F_21c },
    { OPCODES::OP_SPUT_CHAR,              INST_FORMATS::F_21c },
    { OPCODES::OP_SPUT_SHORT,             INST_FORMATS::F_21c },
    { OPCODES::OP_INVOKE_VIRTUAL,         INST_FORMATS::F_35c },
    { OPCODES::OP_INVOKE_SUPER,           INST_FORMATS::F_35c },
    { OPCODES::OP_INVOKE_DIRECT,          INST_FORMATS::F_35c },
    { OPCODES::OP_INVOKE_STATIC,          INST_FORMATS::F_35c },
    { OPCODES::OP_INVOKE_INTERFACE,       INST_FORMATS::F_35c },
    { OPCODES::OP_RETURN_VOID_NO_BARRIER, INST_FORMATS::F_10x },
    { OPCODES::OP_INVOKE_VIRTUAL_RANGE,   INST_FORMATS::F_3rc },
    { OPCODES::OP_INVOKE_SUPER_RANGE,     INST_FORMATS::F_3rc },
    { OPCODES::OP_INVOKE_DIRECT_RANGE,    INST_FORMATS::F_3rc },
    { OPCODES::OP_INVOKE_STATIC_RANGE,    INST_FORMATS::F_3rc },
    { OPCODES::OP_INVOKE_INTERFACE_RANGE, INST_FORMATS::F_3rc },
    { OPCODES::OP_NEG_INT,                INST_FORMATS::F_12x },
    { OPCODES::OP_NOT_INT,                INST_FORMATS::F_12x },
    { OPCODES::OP_NEG_LONG,               INST_FORMATS::F_12x },
    { OPCODES::OP_NOT_LONG,               INST_FORMATS::F_12x },
    { OPCODES::OP_NEG_FLOAT,              INST_FORMATS::F_12x },
    { OPCODES::OP_NEG_DOUBLE,             INST_FORMATS::F_12x },
    { OPCODES::OP_INT_TO_LONG,            INST_FORMATS::F_12x },
    { OPCODES::OP_INT_TO_FLOAT,           INST_FORMATS::F_12x },
    { OPCODES::OP_INT_TO_DOUBLE,          INST_FORMATS::F_12x },
    { OPCODES::OP_LONG_TO_INT,            INST_FORMATS::F_12x },
    { OPCODES::OP_LONG_TO_FLOAT,          INST_FORMATS::F_12x },
    { OPCODES::OP_LONG_TO_DOUBLE,         INST_FORMATS::F_12x },
    { OPCODES::OP_FLOAT_TO_INT,           INST_FORMATS::F_12x },
    { OPCODES::OP_FLOAT_TO_LONG,          INST_FORMATS::F_12x },
    { OPCODES::OP_FLOAT_TO_DOUBLE,        INST_FORMATS::F_12x },
    { OPCODES::OP_DOUBLE_TO_INT,          INST_FORMATS::F_12x },
    { OPCODES::OP_DOUBLE_TO_LONG,         INST_FORMATS::F_12x },
    { OPCODES::OP_DOUBLE_TO_FLOAT,        INST_FORMATS::F_12x },
    { OPCODES::OP_INT_TO_BYTE,            INST_FORMATS::F_12x },
    { OPCODES::OP_INT_TO_CHAR,            INST_FORMATS::F_12x },
    { OPCODES::OP_INT_TO_SHORT,           INST_FORMATS::F_12x },
    { OPCODES::OP_ADD_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_SUB_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_MUL_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_DIV_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_REM_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_AND_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_OR_INT,                 INST_FORMATS::F_23x },
    { OPCODES::OP_XOR_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_SHL_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_SHR_INT,                INST_FORMATS::F_23x },
    { OPCODES::OP_USHR_INT,               INST_FORMATS::F_23x },
    { OPCODES::OP_ADD_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_SUB_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_MUL_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_DIV_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_REM_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_AND_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_OR_LONG,                INST_FORMATS::F_23x },
    { OPCODES::OP_XOR_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_SHL_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_SHR_LONG,               INST_FORMATS::F_23x },
    { OPCODES::OP_USHR_LONG,              INST_FORMATS::F_23x },
    { OPCODES::OP_ADD_FLOAT,              INST_FORMATS::F_23x },
    { OPCODES::OP_SUB_FLOAT,              INST_FORMATS::F_23x },
    { OPCODES::OP_MUL_FLOAT,              INST_FORMATS::F_23x },
    { OPCODES::OP_DIV_FLOAT,              INST_FORMATS::F_23x },
    { OPCODES::OP_REM_FLOAT,              INST_FORMATS::F_23x },
    { OPCODES::OP_ADD_DOUBLE,             INST_FORMATS::F_23x },
    { OPCODES::OP_SUB_DOUBLE,             INST_FORMATS::F_23x },
    { OPCODES::OP_MUL_DOUBLE,             INST_FORMATS::F_23x },
    { OPCODES::OP_DIV_DOUBLE,             INST_FORMATS::F_23x },
    { OPCODES::OP_REM_DOUBLE,             INST_FORMATS::F_23x },
    { OPCODES::OP_ADD_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_SUB_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_MUL_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_DIV_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_REM_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_AND_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_OR_INT_2_ADDR,          INST_FORMATS::F_12x },
    { OPCODES::OP_XOR_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_SHL_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_SHR_INT_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_USHR_INT_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_ADD_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_SUB_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_MUL_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_DIV_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_REM_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_AND_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_OR_LONG_2_ADDR,         INST_FORMATS::F_12x },
    { OPCODES::OP_XOR_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_SHL_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_SHR_LONG_2_ADDR,        INST_FORMATS::F_12x },
    { OPCODES::OP_USHR_LONG_2_ADDR,       INST_FORMATS::F_12x },
    { OPCODES::OP_ADD_FLOAT_2_ADDR,       INST_FORMATS::F_12x },
    { OPCODES::OP_SUB_FLOAT_2_ADDR,       INST_FORMATS::F_12x },
    { OPCODES::OP_MUL_FLOAT_2_ADDR,       INST_FORMATS::F_12x },
    { OPCODES::OP_DIV_FLOAT_2_ADDR,       INST_FORMATS::F_12x },
    { OPCODES::OP_REM_FLOAT_2_ADDR,       INST_FORMATS::F_12x },
    { OPCODES::OP_ADD_DOUBLE_2_ADDR,      INST_FORMATS::F_12x },
    { OPCODES::OP_SUB_DOUBLE_2_ADDR,      INST_FORMATS::F_12x },
    { OPCODES::OP_MUL_DOUBLE_2_ADDR,      INST_FORMATS::F_12x },
    { OPCODES::OP_DIV_DOUBLE_2_ADDR,      INST_FORMATS::F_12x },
    { OPCODES::OP_REM_DOUBLE_2_ADDR,      INST_FORMATS::F_12x },
    { OPCODES::OP_ADD_INT_LIT_16,         INST_FORMATS::F_22s },
    { OPCODES::OP_RSUB_INT,               INST_FORMATS::F_22s },
    { OPCODES::OP_MUL_INT_LIT_16,         INST_FORMATS::F_22s },
    { OPCODES::OP_DIV_INT_LIT_16,         INST_FORMATS::F_22s },
    { OPCODES::OP_REM_INT_LIT_16,         INST_FORMATS::F_22s },
    { OPCODES::OP_AND_INT_LIT_16,         INST_FORMATS::F_22s },
    { OPCODES::OP_OR_INT_LIT_16,          INST_FORMATS::F_22s },
    { OPCODES::OP_XOR_INT_LIT_16,         INST_FORMATS::F_22s },
    { OPCODES::OP_ADD_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_RSUB_INT_LIT_8,         INST_FORMATS::F_22b },
    { OPCODES::OP_MUL_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_DIV_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_REM_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_AND_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_OR_INT_LIT_8,           INST_FORMATS::F_22b },
    { OPCODES::OP_XOR_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_SHL_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_SHR_INT_LIT_8,          INST_FORMATS::F_22b },
    { OPCODES::OP_USHR_INT_LIT_8,         INST_FORMATS::F_22b },

    { OPCODES::OP_INVOKE_POLYMORPHIC,       INST_FORMATS::F_45cc },
    { OPCODES::OP_INVOKE_POLYMORPHIC_RANGE, INST_FORMATS::F_4rcc },
    { OPCODES::OP_INVOKE_CUSTOM,            INST_FORMATS::F_35c  },
    { OPCODES::OP_INVOKE_CUSTOM_RANGE,      INST_FORMATS::F_3rc  },

    { OPCODES::OP_CONST_METHOD_HANDLE,        INST_FORMATS::F_21c },
    { OPCODES::OP_CONST_METHOD_TYPE,          INST_FORMATS::F_21c },

    { OPCODES::OP_IGET_QUICK,                 INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_WIDE_QUICK,            INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_OBJECT_QUICK,          INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_QUICK,                 INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_WIDE_QUICK,            INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_OBJECT_QUICK,          INST_FORMATS::F_22c },
    { OPCODES::OP_INVOKE_VIRTUAL_QUICK,       INST_FORMATS::F_35c },
    { OPCODES::OP_INVOKE_VIRTUAL_RANGE_QUICK, INST_FORMATS::F_3rc },
    { OPCODES::OP_IPUT_BOOLEAN_QUICK,         INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_BYTE_QUICK,            INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_CHAR_QUICK,            INST_FORMATS::F_22c },
    { OPCODES::OP_IPUT_SHORT_QUICK,           INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_BOOLEAN_QUICK,         INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_BYTE_QUICK,            INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_CHAR_QUICK,            INST_FORMATS::F_22c },
    { OPCODES::OP_IGET_SHORT_QUICK,           INST_FORMATS::F_22c },

  };

  auto   it  = size_map.find(op);
  //if (it == std::end(size_map)) {
  //  std::cout << std::hex << "OP: " << op << '\n';
  //}
  return it == size_map.end() ? INST_FORMATS::F_00x : it->second;
}

size_t inst_size_from_format(INST_FORMATS fmt) {
  static const std::map<INST_FORMATS, size_t> size_map {
    { INST_FORMATS::F_00x,   SIZE_MAX  },
    { INST_FORMATS::F_10x,   2    },
    { INST_FORMATS::F_12x,   2    },
    { INST_FORMATS::F_11n,   2    },
    { INST_FORMATS::F_11x,   2    },
    { INST_FORMATS::F_10t,   2    },
    { INST_FORMATS::F_20t,   4    },
    { INST_FORMATS::F_20bc,  4    },
    { INST_FORMATS::F_22x,   4    },
    { INST_FORMATS::F_21t,   4    },
    { INST_FORMATS::F_21s,   4    },
    { INST_FORMATS::F_21h,   4    },
    { INST_FORMATS::F_21c,   4    },
    { INST_FORMATS::F_23x,   4    },
    { INST_FORMATS::F_22b,   4    },
    { INST_FORMATS::F_22t,   4    },
    { INST_FORMATS::F_22s,   4    },
    { INST_FORMATS::F_22c,   4    },
    { INST_FORMATS::F_22cs,  4    },
    { INST_FORMATS::F_30t,   6    },
    { INST_FORMATS::F_32x,   6    },
    { INST_FORMATS::F_31i,   6    },
    { INST_FORMATS::F_31t,   6    },
    { INST_FORMATS::F_31c,   6    },
    { INST_FORMATS::F_35c,   6    },
    { INST_FORMATS::F_35ms,  6    },
    { INST_FORMATS::F_35mi,  6    },
    { INST_FORMATS::F_3rc,   6    },
    { INST_FORMATS::F_3rms,  6    },
    { INST_FORMATS::F_3rmi,  6    },
    { INST_FORMATS::F_45cc,  8    },
    { INST_FORMATS::F_4rcc,  8    },
    { INST_FORMATS::F_51l,   10   },
  };

  auto   it  = size_map.find(fmt);
  return it == size_map.end() ? 0 : it->second;
}

size_t inst_size_from_opcode(OPCODES op) {
  return inst_size_from_format(inst_format_from_opcode(op));
}

bool is_switch_array(const uint8_t* ptr, const uint8_t* end) {
  const size_t size = end - ptr;
  const size_t underlying_struct_size = std::min({
      sizeof(packed_switch),
      sizeof(sparse_switch),
      sizeof(fill_array_data)});

  if (size < underlying_struct_size) {
    return false;
  }

  const auto opcode = static_cast<OPCODES>(*ptr);
  const bool valid_opcode =
    opcode == OPCODES::OP_NOP ||
    opcode == OPCODES::OP_RETURN_VOID ||
    opcode == OPCODES::OP_RETURN_VOID_NO_BARRIER;

  if (!valid_opcode) {
    return false;
  }

  const auto ident = static_cast<SWITCH_ARRAY_IDENT>(static_cast<uint16_t>(ptr[1] << 8) | opcode);

  switch(ident) {
    case SWITCH_ARRAY_IDENT::IDENT_PACKED_SWITCH:
    case SWITCH_ARRAY_IDENT::IDENT_SPARSE_SWITCH:
    case SWITCH_ARRAY_IDENT::IDENT_FILL_ARRAY:
      {
        return true;
      }
    default:
      {
        return false;
      }
  }
  return false;
}

size_t switch_array_size(const uint8_t* ptr, const uint8_t* end) {
  if (!is_switch_array(ptr, end)) {
    return SIZE_MAX;
  }

  const auto opcode = static_cast<OPCODES>(*ptr);
  const auto ident = static_cast<SWITCH_ARRAY_IDENT>(static_cast<uint16_t>(ptr[1] << 8) | opcode);
  const uint8_t* ptr_struct = ptr;
  size_t size = 0;

  switch(ident) {
    case SWITCH_ARRAY_IDENT::IDENT_PACKED_SWITCH:
      {
        size_t nb_elements = reinterpret_cast<const packed_switch*>(ptr_struct)->size;
        size += sizeof(packed_switch);
        size += nb_elements * sizeof(uint32_t);
        return size;
      }
    case SWITCH_ARRAY_IDENT::IDENT_SPARSE_SWITCH:
      {
        size_t nb_elements = reinterpret_cast<const sparse_switch*>(ptr_struct)->size;
        size += sizeof(sparse_switch);
        size += 2 *nb_elements * sizeof(uint32_t);
        return size;
      }
    case SWITCH_ARRAY_IDENT::IDENT_FILL_ARRAY:
      {
        size_t nb_elements = reinterpret_cast<const fill_array_data*>(ptr_struct)->size;
        size_t width       = reinterpret_cast<const fill_array_data*>(ptr_struct)->element_width;
        size_t data_size   = nb_elements * width;
        data_size += data_size % 2;

        size += sizeof(fill_array_data);
        size += data_size;
        return size;
      }
    default:
      {
        return SIZE_MAX;
      }
  }

  return size;

}

}
}

