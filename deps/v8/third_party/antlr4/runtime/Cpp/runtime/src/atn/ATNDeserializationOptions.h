/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "antlr4-common.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC ATNDeserializationOptions {
 private:
  static ATNDeserializationOptions defaultOptions;

  bool readOnly;
  bool verifyATN;
  bool generateRuleBypassTransitions;

 public:
  ATNDeserializationOptions();
  ATNDeserializationOptions(ATNDeserializationOptions* options);
  ATNDeserializationOptions(ATNDeserializationOptions const&) = default;
  virtual ~ATNDeserializationOptions();
  ATNDeserializationOptions& operator=(ATNDeserializationOptions const&) =
      default;

  static const ATNDeserializationOptions& getDefaultOptions();

  bool isReadOnly();

  void makeReadOnly();

  bool isVerifyATN();

  void setVerifyATN(bool verify);

  bool isGenerateRuleBypassTransitions();

  void setGenerateRuleBypassTransitions(bool generate);

 protected:
  virtual void throwIfReadOnly();

 private:
  void InitializeInstanceFields();
};

}  // namespace atn
}  // namespace antlr4
