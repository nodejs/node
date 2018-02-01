#ifndef SRC_STRING_DECODER_INL_H_
#define SRC_STRING_DECODER_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "string_decoder.h"
#include "util.h"

namespace node {

inline void StringDecoder::SetEncoding(enum encoding encoding) {
  state_[kBufferedBytes] = 0;
  state_[kMissingBytes] = 0;
  state_[kEncodingField] = encoding;
}

inline enum encoding StringDecoder::Encoding() const {
  return static_cast<enum encoding>(state_[kEncodingField]);
}

inline unsigned StringDecoder::BufferedBytes() const {
  return state_[kBufferedBytes];
}

inline unsigned StringDecoder::MissingBytes() const {
  return state_[kMissingBytes];
}

inline char* StringDecoder::IncompleteCharacterBuffer() {
  return reinterpret_cast<char*>(state_ + kIncompleteCharactersStart);
}


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif   // SRC_STRING_DECODER_INL_H_
