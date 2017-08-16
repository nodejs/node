// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPRESSION_TYPE_COLLECTOR_MACROS_H_
#define V8_EXPRESSION_TYPE_COLLECTOR_MACROS_H_

#define CHECK_TYPES_BEGIN \
  {                       \
    size_t index = 0;     \
    int depth = 0;

#define CHECK_TYPES_END          \
  CHECK_EQ(index, types.size()); \
  }

#ifdef DEBUG
#define CHECK_TYPE(type)                    \
  if (!types[index].bounds.Narrows(type)) { \
    fprintf(stderr, "Expected:\n");         \
    fprintf(stderr, "  lower: ");           \
    type.lower->Print();                    \
    fprintf(stderr, "  upper: ");           \
    type.upper->Print();                    \
    fprintf(stderr, "Actual:\n");           \
    fprintf(stderr, "  lower: ");           \
    types[index].bounds.lower->Print();     \
    fprintf(stderr, "  upper: ");           \
    types[index].bounds.upper->Print();     \
  }                                         \
  CHECK(types[index].bounds.Narrows(type));
#else
#define CHECK_TYPE(type) CHECK(types[index].bounds.Narrows(type));
#endif

#define CHECK_EXPR(ekind, type)                  \
  CHECK_LT(index, types.size());                 \
  CHECK(strcmp(#ekind, types[index].kind) == 0); \
  CHECK_EQ(depth, types[index].depth);           \
  CHECK_TYPE(type);                              \
  for (int j = (++depth, ++index, 0); j < 1 ? 1 : (--depth, 0); ++j)

#define CHECK_VAR(vname, type)                                     \
  CHECK_EXPR(VariableProxy, type);                                 \
  CHECK_EQ(#vname, std::string(types[index - 1].name->raw_data(),  \
                               types[index - 1].name->raw_data() + \
                                   types[index - 1].name->byte_length()));

#define CHECK_SKIP()                                             \
  {                                                              \
    ++index;                                                     \
    while (index < types.size() && types[index].depth > depth) { \
      ++index;                                                   \
    }                                                            \
  }

#endif  // V8_EXPRESSION_TYPE_COLLECTOR_MACROS_H_
