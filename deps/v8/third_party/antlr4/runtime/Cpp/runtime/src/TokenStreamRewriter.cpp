/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "Exceptions.h"
#include "Token.h"
#include "TokenStream.h"
#include "misc/Interval.h"

#include "TokenStreamRewriter.h"

using namespace antlr4;

using antlr4::misc::Interval;

TokenStreamRewriter::RewriteOperation::RewriteOperation(
    TokenStreamRewriter* outerInstance_, size_t index_)
    : outerInstance(outerInstance_) {
  InitializeInstanceFields();
  this->index = index_;
}

TokenStreamRewriter::RewriteOperation::RewriteOperation(
    TokenStreamRewriter* outerInstance_, size_t index_,
    const std::string& text_)
    : outerInstance(outerInstance_) {
  InitializeInstanceFields();
  this->index = index_;
  this->text = text_;
}

TokenStreamRewriter::RewriteOperation::~RewriteOperation() {}

size_t TokenStreamRewriter::RewriteOperation::execute(std::string* /*buf*/) {
  return index;
}

std::string TokenStreamRewriter::RewriteOperation::toString() {
  std::string opName = "TokenStreamRewriter";
  size_t dollarIndex = opName.find('$');
  opName = opName.substr(dollarIndex + 1, opName.length() - (dollarIndex + 1));
  return "<" + opName + "@" +
         outerInstance->tokens->get(dollarIndex)->getText() + ":\"" + text +
         "\">";
}

void TokenStreamRewriter::RewriteOperation::InitializeInstanceFields() {
  instructionIndex = 0;
  index = 0;
}

TokenStreamRewriter::InsertBeforeOp::InsertBeforeOp(
    TokenStreamRewriter* outerInstance_, size_t index_,
    const std::string& text_)
    : RewriteOperation(outerInstance_, index_, text_),
      outerInstance(outerInstance_) {}

size_t TokenStreamRewriter::InsertBeforeOp::execute(std::string* buf) {
  buf->append(text);
  if (outerInstance->tokens->get(index)->getType() != Token::EOF) {
    buf->append(outerInstance->tokens->get(index)->getText());
  }
  return index + 1;
}

TokenStreamRewriter::ReplaceOp::ReplaceOp(TokenStreamRewriter* outerInstance_,
                                          size_t from, size_t to,
                                          const std::string& text)
    : RewriteOperation(outerInstance_, from, text),
      outerInstance(outerInstance_) {
  InitializeInstanceFields();
  lastIndex = to;
}

size_t TokenStreamRewriter::ReplaceOp::execute(std::string* buf) {
  buf->append(text);
  return lastIndex + 1;
}

std::string TokenStreamRewriter::ReplaceOp::toString() {
  if (text.empty()) {
    return "<DeleteOp@" + outerInstance->tokens->get(index)->getText() + ".." +
           outerInstance->tokens->get(lastIndex)->getText() + ">";
  }
  return "<ReplaceOp@" + outerInstance->tokens->get(index)->getText() + ".." +
         outerInstance->tokens->get(lastIndex)->getText() + ":\"" + text +
         "\">";
}

void TokenStreamRewriter::ReplaceOp::InitializeInstanceFields() {
  lastIndex = 0;
}

//------------------ TokenStreamRewriter
//-------------------------------------------------------------------------------

const std::string TokenStreamRewriter::DEFAULT_PROGRAM_NAME = "default";

TokenStreamRewriter::TokenStreamRewriter(TokenStream* tokens_)
    : tokens(tokens_) {
  _programs[DEFAULT_PROGRAM_NAME].reserve(PROGRAM_INIT_SIZE);
}

TokenStreamRewriter::~TokenStreamRewriter() {
  for (auto program : _programs) {
    for (auto operation : program.second) {
      delete operation;
    }
  }
}

TokenStream* TokenStreamRewriter::getTokenStream() { return tokens; }

void TokenStreamRewriter::rollback(size_t instructionIndex) {
  rollback(DEFAULT_PROGRAM_NAME, instructionIndex);
}

void TokenStreamRewriter::rollback(const std::string& programName,
                                   size_t instructionIndex) {
  std::vector<RewriteOperation*> is = _programs[programName];
  if (is.size() > 0) {
    _programs.insert({programName, std::vector<RewriteOperation*>(
                                       is.begin() + MIN_TOKEN_INDEX,
                                       is.begin() + instructionIndex)});
  }
}

void TokenStreamRewriter::deleteProgram() {
  deleteProgram(DEFAULT_PROGRAM_NAME);
}

void TokenStreamRewriter::deleteProgram(const std::string& programName) {
  rollback(programName, MIN_TOKEN_INDEX);
}

void TokenStreamRewriter::insertAfter(Token* t, const std::string& text) {
  insertAfter(DEFAULT_PROGRAM_NAME, t, text);
}

void TokenStreamRewriter::insertAfter(size_t index, const std::string& text) {
  insertAfter(DEFAULT_PROGRAM_NAME, index, text);
}

void TokenStreamRewriter::insertAfter(const std::string& programName, Token* t,
                                      const std::string& text) {
  insertAfter(programName, t->getTokenIndex(), text);
}

void TokenStreamRewriter::insertAfter(const std::string& programName,
                                      size_t index, const std::string& text) {
  // to insert after, just insert before next index (even if past end)
  insertBefore(programName, index + 1, text);
}

void TokenStreamRewriter::insertBefore(Token* t, const std::string& text) {
  insertBefore(DEFAULT_PROGRAM_NAME, t, text);
}

void TokenStreamRewriter::insertBefore(size_t index, const std::string& text) {
  insertBefore(DEFAULT_PROGRAM_NAME, index, text);
}

void TokenStreamRewriter::insertBefore(const std::string& programName, Token* t,
                                       const std::string& text) {
  insertBefore(programName, t->getTokenIndex(), text);
}

void TokenStreamRewriter::insertBefore(const std::string& programName,
                                       size_t index, const std::string& text) {
  RewriteOperation* op =
      new InsertBeforeOp(this, index, text); /* mem-check: deleted in d-tor */
  std::vector<RewriteOperation*>& rewrites = getProgram(programName);
  op->instructionIndex = rewrites.size();
  rewrites.push_back(op);
}

void TokenStreamRewriter::replace(size_t index, const std::string& text) {
  replace(DEFAULT_PROGRAM_NAME, index, index, text);
}

void TokenStreamRewriter::replace(size_t from, size_t to,
                                  const std::string& text) {
  replace(DEFAULT_PROGRAM_NAME, from, to, text);
}

void TokenStreamRewriter::replace(Token* indexT, const std::string& text) {
  replace(DEFAULT_PROGRAM_NAME, indexT, indexT, text);
}

void TokenStreamRewriter::replace(Token* from, Token* to,
                                  const std::string& text) {
  replace(DEFAULT_PROGRAM_NAME, from, to, text);
}

void TokenStreamRewriter::replace(const std::string& programName, size_t from,
                                  size_t to, const std::string& text) {
  if (from > to || to >= tokens->size()) {
    throw IllegalArgumentException(
        "replace: range invalid: " + std::to_string(from) + ".." +
        std::to_string(to) + "(size = " + std::to_string(tokens->size()) + ")");
  }
  RewriteOperation* op =
      new ReplaceOp(this, from, to, text); /* mem-check: deleted in d-tor */
  std::vector<RewriteOperation*>& rewrites = getProgram(programName);
  op->instructionIndex = rewrites.size();
  rewrites.push_back(op);
}

void TokenStreamRewriter::replace(const std::string& programName, Token* from,
                                  Token* to, const std::string& text) {
  replace(programName, from->getTokenIndex(), to->getTokenIndex(), text);
}

void TokenStreamRewriter::Delete(size_t index) {
  Delete(DEFAULT_PROGRAM_NAME, index, index);
}

void TokenStreamRewriter::Delete(size_t from, size_t to) {
  Delete(DEFAULT_PROGRAM_NAME, from, to);
}

void TokenStreamRewriter::Delete(Token* indexT) {
  Delete(DEFAULT_PROGRAM_NAME, indexT, indexT);
}

void TokenStreamRewriter::Delete(Token* from, Token* to) {
  Delete(DEFAULT_PROGRAM_NAME, from, to);
}

void TokenStreamRewriter::Delete(const std::string& programName, size_t from,
                                 size_t to) {
  std::string nullString;
  replace(programName, from, to, nullString);
}

void TokenStreamRewriter::Delete(const std::string& programName, Token* from,
                                 Token* to) {
  std::string nullString;
  replace(programName, from, to, nullString);
}

size_t TokenStreamRewriter::getLastRewriteTokenIndex() {
  return getLastRewriteTokenIndex(DEFAULT_PROGRAM_NAME);
}

size_t TokenStreamRewriter::getLastRewriteTokenIndex(
    const std::string& programName) {
  if (_lastRewriteTokenIndexes.find(programName) ==
      _lastRewriteTokenIndexes.end()) {
    return INVALID_INDEX;
  }
  return _lastRewriteTokenIndexes[programName];
}

void TokenStreamRewriter::setLastRewriteTokenIndex(
    const std::string& programName, size_t i) {
  _lastRewriteTokenIndexes.insert({programName, i});
}

std::vector<TokenStreamRewriter::RewriteOperation*>&
TokenStreamRewriter::getProgram(const std::string& name) {
  auto iterator = _programs.find(name);
  if (iterator == _programs.end()) {
    return initializeProgram(name);
  }
  return iterator->second;
}

std::vector<TokenStreamRewriter::RewriteOperation*>&
TokenStreamRewriter::initializeProgram(const std::string& name) {
  _programs[name].reserve(PROGRAM_INIT_SIZE);
  return _programs[name];
}

std::string TokenStreamRewriter::getText() {
  return getText(DEFAULT_PROGRAM_NAME, Interval(0UL, tokens->size() - 1));
}

std::string TokenStreamRewriter::getText(std::string programName) {
  return getText(programName, Interval(0UL, tokens->size() - 1));
}

std::string TokenStreamRewriter::getText(const Interval& interval) {
  return getText(DEFAULT_PROGRAM_NAME, interval);
}

std::string TokenStreamRewriter::getText(const std::string& programName,
                                         const Interval& interval) {
  std::vector<TokenStreamRewriter::RewriteOperation*>& rewrites =
      _programs[programName];
  size_t start = interval.a;
  size_t stop = interval.b;

  // ensure start/end are in range
  if (stop > tokens->size() - 1) {
    stop = tokens->size() - 1;
  }
  if (start == INVALID_INDEX) {
    start = 0;
  }

  if (rewrites.empty() || rewrites.empty()) {
    return tokens->getText(interval);  // no instructions to execute
  }
  std::string buf;

  // First, optimize instruction stream
  std::unordered_map<size_t, TokenStreamRewriter::RewriteOperation*> indexToOp =
      reduceToSingleOperationPerIndex(rewrites);

  // Walk buffer, executing instructions and emitting tokens
  size_t i = start;
  while (i <= stop && i < tokens->size()) {
    RewriteOperation* op = indexToOp[i];
    indexToOp.erase(i);  // remove so any left have index size-1
    Token* t = tokens->get(i);
    if (op == nullptr) {
      // no operation at that index, just dump token
      if (t->getType() != Token::EOF) {
        buf.append(t->getText());
      }
      i++;  // move to next token
    } else {
      i = op->execute(&buf);  // execute operation and skip
    }
  }

  // include stuff after end if it's last index in buffer
  // So, if they did an insertAfter(lastValidIndex, "foo"), include
  // foo if end==lastValidIndex.
  if (stop == tokens->size() - 1) {
    // Scan any remaining operations after last token
    // should be included (they will be inserts).
    for (auto op : indexToOp) {
      if (op.second->index >= tokens->size() - 1) {
        buf.append(op.second->text);
      }
    }
  }
  return buf;
}

std::unordered_map<size_t, TokenStreamRewriter::RewriteOperation*>
TokenStreamRewriter::reduceToSingleOperationPerIndex(
    std::vector<TokenStreamRewriter::RewriteOperation*>& rewrites) {
  // WALK REPLACES
  for (size_t i = 0; i < rewrites.size(); ++i) {
    TokenStreamRewriter::RewriteOperation* op = rewrites[i];
    ReplaceOp* rop = dynamic_cast<ReplaceOp*>(op);
    if (rop == nullptr) continue;

    // Wipe prior inserts within range
    std::vector<InsertBeforeOp*> inserts =
        getKindOfOps<InsertBeforeOp>(rewrites, i);
    for (auto iop : inserts) {
      if (iop->index == rop->index) {
        // E.g., insert before 2, delete 2..2; update replace
        // text to include insert before, kill insert
        delete rewrites[iop->instructionIndex];
        rewrites[iop->instructionIndex] = nullptr;
        rop->text = iop->text + (!rop->text.empty() ? rop->text : "");
      } else if (iop->index > rop->index && iop->index <= rop->lastIndex) {
        // delete insert as it's a no-op.
        delete rewrites[iop->instructionIndex];
        rewrites[iop->instructionIndex] = nullptr;
      }
    }
    // Drop any prior replaces contained within
    std::vector<ReplaceOp*> prevReplaces = getKindOfOps<ReplaceOp>(rewrites, i);
    for (auto prevRop : prevReplaces) {
      if (prevRop->index >= rop->index &&
          prevRop->lastIndex <= rop->lastIndex) {
        // delete replace as it's a no-op.
        delete rewrites[prevRop->instructionIndex];
        rewrites[prevRop->instructionIndex] = nullptr;
        continue;
      }
      // throw exception unless disjoint or identical
      bool disjoint =
          prevRop->lastIndex < rop->index || prevRop->index > rop->lastIndex;
      bool same =
          prevRop->index == rop->index && prevRop->lastIndex == rop->lastIndex;
      // Delete special case of replace (text==null):
      // D.i-j.u D.x-y.v    | boundaries overlap    combine to
      // max(min)..max(right)
      if (prevRop->text.empty() && rop->text.empty() && !disjoint) {
        delete rewrites[prevRop->instructionIndex];
        rewrites[prevRop->instructionIndex] = nullptr;  // kill first delete
        rop->index = std::min(prevRop->index, rop->index);
        rop->lastIndex = std::max(prevRop->lastIndex, rop->lastIndex);
        std::cout << "new rop " << rop << std::endl;
      } else if (!disjoint && !same) {
        throw IllegalArgumentException(
            "replace op boundaries of " + rop->toString() +
            " overlap with previous " + prevRop->toString());
      }
    }
  }

  // WALK INSERTS
  for (size_t i = 0; i < rewrites.size(); i++) {
    InsertBeforeOp* iop = dynamic_cast<InsertBeforeOp*>(rewrites[i]);
    if (iop == nullptr) continue;

    // combine current insert with prior if any at same index

    std::vector<InsertBeforeOp*> prevInserts =
        getKindOfOps<InsertBeforeOp>(rewrites, i);
    for (auto prevIop : prevInserts) {
      if (prevIop->index == iop->index) {  // combine objects
                                           // convert to strings...we're in
                                           // process of toString'ing whole
                                           // token buffer so no lazy eval issue
                                           // with any templates
        iop->text = catOpText(&iop->text, &prevIop->text);
        // delete redundant prior insert
        delete rewrites[prevIop->instructionIndex];
        rewrites[prevIop->instructionIndex] = nullptr;
      }
    }
    // look for replaces where iop.index is in range; error
    std::vector<ReplaceOp*> prevReplaces = getKindOfOps<ReplaceOp>(rewrites, i);
    for (auto rop : prevReplaces) {
      if (iop->index == rop->index) {
        rop->text = catOpText(&iop->text, &rop->text);
        delete rewrites[i];
        rewrites[i] = nullptr;  // delete current insert
        continue;
      }
      if (iop->index >= rop->index && iop->index <= rop->lastIndex) {
        throw IllegalArgumentException("insert op " + iop->toString() +
                                       " within boundaries of previous " +
                                       rop->toString());
      }
    }
  }

  std::unordered_map<size_t, TokenStreamRewriter::RewriteOperation*> m;
  for (TokenStreamRewriter::RewriteOperation* op : rewrites) {
    if (op == nullptr) {  // ignore deleted ops
      continue;
    }
    if (m.count(op->index) > 0) {
      throw RuntimeException("should only be one op per index");
    }
    m[op->index] = op;
  }

  return m;
}

std::string TokenStreamRewriter::catOpText(std::string* a, std::string* b) {
  std::string x = "";
  std::string y = "";
  if (a != nullptr) {
    x = *a;
  }
  if (b != nullptr) {
    y = *b;
  }
  return x + y;
}
