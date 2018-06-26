/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "dfa/DFASerializer.h"

namespace antlr4 {
namespace dfa {

class ANTLR4CPP_PUBLIC LexerDFASerializer : public DFASerializer {
 public:
  LexerDFASerializer(DFA* dfa);
  virtual ~LexerDFASerializer();

 protected:
  virtual std::string getEdgeLabel(size_t i) const override;
};

}  // namespace dfa
}  // namespace antlr4
