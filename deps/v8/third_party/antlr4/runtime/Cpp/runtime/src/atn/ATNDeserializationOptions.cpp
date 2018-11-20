/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/ATNDeserializationOptions.h"

using namespace antlr4::atn;

ATNDeserializationOptions ATNDeserializationOptions::defaultOptions;

ATNDeserializationOptions::ATNDeserializationOptions() {
  InitializeInstanceFields();
}

ATNDeserializationOptions::ATNDeserializationOptions(
    ATNDeserializationOptions* options)
    : ATNDeserializationOptions() {
  this->verifyATN = options->verifyATN;
  this->generateRuleBypassTransitions = options->generateRuleBypassTransitions;
}

ATNDeserializationOptions::~ATNDeserializationOptions() {}

const ATNDeserializationOptions&
ATNDeserializationOptions::getDefaultOptions() {
  return defaultOptions;
}

bool ATNDeserializationOptions::isReadOnly() { return readOnly; }

void ATNDeserializationOptions::makeReadOnly() { readOnly = true; }

bool ATNDeserializationOptions::isVerifyATN() { return verifyATN; }

void ATNDeserializationOptions::setVerifyATN(bool verify) {
  throwIfReadOnly();
  verifyATN = verify;
}

bool ATNDeserializationOptions::isGenerateRuleBypassTransitions() {
  return generateRuleBypassTransitions;
}

void ATNDeserializationOptions::setGenerateRuleBypassTransitions(
    bool generate) {
  throwIfReadOnly();
  generateRuleBypassTransitions = generate;
}

void ATNDeserializationOptions::throwIfReadOnly() {
  if (isReadOnly()) {
    throw "The object is read only.";
  }
}

void ATNDeserializationOptions::InitializeInstanceFields() {
  readOnly = false;
  verifyATN = true;
  generateRuleBypassTransitions = false;
}
