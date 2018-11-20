/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/SingletonPredictionContext.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC EmptyPredictionContext
    : public SingletonPredictionContext {
 public:
  EmptyPredictionContext();

  virtual bool isEmpty() const override;
  virtual size_t size() const override;
  virtual Ref<PredictionContext> getParent(size_t index) const override;
  virtual size_t getReturnState(size_t index) const override;
  virtual std::string toString() const override;

  virtual bool operator==(const PredictionContext& o) const override;
};

}  // namespace atn
}  // namespace antlr4
