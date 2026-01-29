#ifndef SRC_ENCODING_SINGLEBYTE_H_
#define SRC_ENCODING_SINGLEBYTE_H_

#include <cstdint>

namespace node {

constexpr int kXUserDefined = 28;  // Last one, see encoding.js

extern const uint16_t* const* const tSingleByteEncodings;
extern const bool* const fSingleByteEncodings;

}  // namespace node

#endif  // SRC_ENCODING_SINGLEBYTE_H_
