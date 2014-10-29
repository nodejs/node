// Copyright Fedor Indutny and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef DEPS_DEBUGGER_AGENT_SRC_AGENT_H_
#define DEPS_DEBUGGER_AGENT_SRC_AGENT_H_

#include "v8.h"
#include "v8-debug.h"
#include "queue.h"

#include <assert.h>
#include <string.h>

namespace node {
namespace debugger {

class AgentMessage {
 public:
  AgentMessage(uint16_t* val, int length) : length_(length) {
    if (val == NULL) {
      data_ = val;
    } else {
      data_ = new uint16_t[length];
      memcpy(data_, val, length * sizeof(*data_));
    }
  }

  ~AgentMessage() {
    delete[] data_;
    data_ = NULL;
  }

  inline const uint16_t* data() const { return data_; }
  inline int length() const { return length_; }

  QUEUE member;

 private:
  uint16_t* data_;
  int length_;
};

}  // namespace debugger
}  // namespace node

#endif  // DEPS_DEBUGGER_AGENT_SRC_AGENT_H_
