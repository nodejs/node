/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "atn/ATNConfig.h"
#include "atn/ATNConfigSet.h"
#include "atn/SemanticContext.h"
#include "misc/MurmurHash.h"

#include "dfa/DFAState.h"

using namespace antlr4::dfa;
using namespace antlr4::atn;

DFAState::PredPrediction::PredPrediction(const Ref<SemanticContext>& pred,
                                         int alt)
    : pred(pred) {
  InitializeInstanceFields();
  this->alt = alt;
}

DFAState::PredPrediction::~PredPrediction() {}

std::string DFAState::PredPrediction::toString() {
  return std::string("(") + pred->toString() + ", " + std::to_string(alt) + ")";
}

void DFAState::PredPrediction::InitializeInstanceFields() { alt = 0; }

DFAState::DFAState() { InitializeInstanceFields(); }

DFAState::DFAState(int state) : DFAState() { stateNumber = state; }

DFAState::DFAState(std::unique_ptr<ATNConfigSet> configs_) : DFAState() {
  configs = std::move(configs_);
}

DFAState::~DFAState() {
  for (auto predicate : predicates) {
    delete predicate;
  }
}

std::set<size_t> DFAState::getAltSet() {
  std::set<size_t> alts;
  if (configs != nullptr) {
    for (size_t i = 0; i < configs->size(); i++) {
      alts.insert(configs->get(i)->alt);
    }
  }
  return alts;
}

size_t DFAState::hashCode() const {
  size_t hash = misc::MurmurHash::initialize(7);
  hash = misc::MurmurHash::update(hash, configs->hashCode());
  hash = misc::MurmurHash::finish(hash, 1);
  return hash;
}

bool DFAState::operator==(const DFAState& o) const {
  // compare set of ATN configurations in this set with other
  if (this == &o) {
    return true;
  }

  return *configs == *o.configs;
}

std::string DFAState::toString() {
  std::stringstream ss;
  ss << stateNumber;
  if (configs) {
    ss << ":" << configs->toString();
  }
  if (isAcceptState) {
    ss << " => ";
    if (!predicates.empty()) {
      for (size_t i = 0; i < predicates.size(); i++) {
        ss << predicates[i]->toString();
      }
    } else {
      ss << prediction;
    }
  }
  return ss.str();
}

void DFAState::InitializeInstanceFields() {
  stateNumber = -1;
  isAcceptState = false;
  prediction = 0;
  requiresFullContext = false;
}
