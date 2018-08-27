/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "atn/PredictionContext.h"

namespace antlr4 {
namespace atn {

class ANTLR4CPP_PUBLIC SingletonPredictionContext : public PredictionContext {
 public:
  // Usually a parent is linked via a weak ptr. Not so here as we have kinda
  // reverse reference chain. There are no child contexts stored here and often
  // the parent context is left dangling when it's owning ATNState is released.
  // In order to avoid having this context released as well (leaving all other
  // contexts which got this one as parent with a null reference) we use a
  // shared_ptr here instead, to keep those left alone parent contexts alive.
  const Ref<PredictionContext> parent;
  const size_t returnState;

  SingletonPredictionContext(Ref<PredictionContext> const& parent,
                             size_t returnState);
  virtual ~SingletonPredictionContext();

  static Ref<SingletonPredictionContext> create(
      Ref<PredictionContext> const& parent, size_t returnState);

  virtual size_t size() const override;
  virtual Ref<PredictionContext> getParent(size_t index) const override;
  virtual size_t getReturnState(size_t index) const override;
  virtual bool operator==(const PredictionContext& o) const override;
  virtual std::string toString() const override;
};

}  // namespace atn
}  // namespace antlr4
