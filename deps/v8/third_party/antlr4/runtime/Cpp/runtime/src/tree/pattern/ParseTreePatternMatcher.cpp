/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "BailErrorStrategy.h"
#include "CommonTokenStream.h"
#include "Lexer.h"
#include "ParserInterpreter.h"
#include "ParserRuleContext.h"
#include "atn/ATN.h"
#include "tree/TerminalNode.h"
#include "tree/pattern/ParseTreeMatch.h"
#include "tree/pattern/ParseTreePattern.h"
#include "tree/pattern/RuleTagToken.h"
#include "tree/pattern/TagChunk.h"
#include "tree/pattern/TokenTagToken.h"

#include "ANTLRInputStream.h"
#include "Exceptions.h"
#include "ListTokenSource.h"
#include "support/Arrays.h"
#include "support/CPPUtils.h"
#include "support/StringUtils.h"
#include "tree/pattern/TextChunk.h"

#include "tree/pattern/ParseTreePatternMatcher.h"

using namespace antlr4;
using namespace antlr4::tree;
using namespace antlr4::tree::pattern;
using namespace antlrcpp;

ParseTreePatternMatcher::CannotInvokeStartRule::CannotInvokeStartRule(
    const RuntimeException& e)
    : RuntimeException(e.what()) {}

ParseTreePatternMatcher::CannotInvokeStartRule::~CannotInvokeStartRule() {}

ParseTreePatternMatcher::StartRuleDoesNotConsumeFullPattern::
    ~StartRuleDoesNotConsumeFullPattern() {}

ParseTreePatternMatcher::ParseTreePatternMatcher(Lexer* lexer, Parser* parser)
    : _lexer(lexer), _parser(parser) {
  InitializeInstanceFields();
}

ParseTreePatternMatcher::~ParseTreePatternMatcher() {}

void ParseTreePatternMatcher::setDelimiters(const std::string& start,
                                            const std::string& stop,
                                            const std::string& escapeLeft) {
  if (start.empty()) {
    throw IllegalArgumentException("start cannot be null or empty");
  }

  if (stop.empty()) {
    throw IllegalArgumentException("stop cannot be null or empty");
  }

  _start = start;
  _stop = stop;
  _escape = escapeLeft;
}

bool ParseTreePatternMatcher::matches(ParseTree* tree,
                                      const std::string& pattern,
                                      int patternRuleIndex) {
  ParseTreePattern p = compile(pattern, patternRuleIndex);
  return matches(tree, p);
}

bool ParseTreePatternMatcher::matches(ParseTree* tree,
                                      const ParseTreePattern& pattern) {
  std::map<std::string, std::vector<ParseTree*>> labels;
  ParseTree* mismatchedNode = matchImpl(tree, pattern.getPatternTree(), labels);
  return mismatchedNode == nullptr;
}

ParseTreeMatch ParseTreePatternMatcher::match(ParseTree* tree,
                                              const std::string& pattern,
                                              int patternRuleIndex) {
  ParseTreePattern p = compile(pattern, patternRuleIndex);
  return match(tree, p);
}

ParseTreeMatch ParseTreePatternMatcher::match(ParseTree* tree,
                                              const ParseTreePattern& pattern) {
  std::map<std::string, std::vector<ParseTree*>> labels;
  tree::ParseTree* mismatchedNode =
      matchImpl(tree, pattern.getPatternTree(), labels);
  return ParseTreeMatch(tree, pattern, labels, mismatchedNode);
}

ParseTreePattern ParseTreePatternMatcher::compile(const std::string& pattern,
                                                  int patternRuleIndex) {
  ListTokenSource tokenSrc(tokenize(pattern));
  CommonTokenStream tokens(&tokenSrc);

  ParserInterpreter parserInterp(
      _parser->getGrammarFileName(), _parser->getVocabulary(),
      _parser->getRuleNames(), _parser->getATNWithBypassAlts(), &tokens);

  ParserRuleContext* tree = nullptr;
  try {
    parserInterp.setErrorHandler(std::make_shared<BailErrorStrategy>());
    tree = parserInterp.parse(patternRuleIndex);
  } catch (ParseCancellationException& e) {
#if defined(_MSC_FULL_VER) && _MSC_FULL_VER < 190023026
    // rethrow_if_nested is not available before VS 2015.
    throw e;
#else
    std::rethrow_if_nested(e);  // Unwrap the nested exception.
#endif
  } catch (RecognitionException& re) {
    throw re;
  } catch (std::exception& e) {
    // throw_with_nested is not available before VS 2015.
    throw e;
  }

  // Make sure tree pattern compilation checks for a complete parse
  if (tokens.LA(1) != Token::EOF) {
    throw StartRuleDoesNotConsumeFullPattern();
  }

  return ParseTreePattern(this, pattern, patternRuleIndex, tree);
}

Lexer* ParseTreePatternMatcher::getLexer() { return _lexer; }

Parser* ParseTreePatternMatcher::getParser() { return _parser; }

ParseTree* ParseTreePatternMatcher::matchImpl(
    ParseTree* tree, ParseTree* patternTree,
    std::map<std::string, std::vector<ParseTree*>>& labels) {
  if (tree == nullptr) {
    throw IllegalArgumentException("tree cannot be nul");
  }

  if (patternTree == nullptr) {
    throw IllegalArgumentException("patternTree cannot be nul");
  }

  // x and <ID>, x and y, or x and x; or could be mismatched types
  if (is<TerminalNode*>(tree) && is<TerminalNode*>(patternTree)) {
    TerminalNode* t1 = dynamic_cast<TerminalNode*>(tree);
    TerminalNode* t2 = dynamic_cast<TerminalNode*>(patternTree);

    ParseTree* mismatchedNode = nullptr;
    // both are tokens and they have same type
    if (t1->getSymbol()->getType() == t2->getSymbol()->getType()) {
      if (is<TokenTagToken*>(t2->getSymbol())) {  // x and <ID>
        TokenTagToken* tokenTagToken =
            dynamic_cast<TokenTagToken*>(t2->getSymbol());

        // track label->list-of-nodes for both token name and label (if any)
        labels[tokenTagToken->getTokenName()].push_back(tree);
        if (tokenTagToken->getLabel() != "") {
          labels[tokenTagToken->getLabel()].push_back(tree);
        }
      } else if (t1->getText() == t2->getText()) {
        // x and x
      } else {
        // x and y
        if (mismatchedNode == nullptr) {
          mismatchedNode = t1;
        }
      }
    } else {
      if (mismatchedNode == nullptr) {
        mismatchedNode = t1;
      }
    }

    return mismatchedNode;
  }

  if (is<ParserRuleContext*>(tree) && is<ParserRuleContext*>(patternTree)) {
    ParserRuleContext* r1 = dynamic_cast<ParserRuleContext*>(tree);
    ParserRuleContext* r2 = dynamic_cast<ParserRuleContext*>(patternTree);
    ParseTree* mismatchedNode = nullptr;

    // (expr ...) and <expr>
    RuleTagToken* ruleTagToken = getRuleTagToken(r2);
    if (ruleTagToken != nullptr) {
      // ParseTreeMatch *m = nullptr; // unused?
      if (r1->getRuleIndex() == r2->getRuleIndex()) {
        // track label->list-of-nodes for both rule name and label (if any)
        labels[ruleTagToken->getRuleName()].push_back(tree);
        if (ruleTagToken->getLabel() != "") {
          labels[ruleTagToken->getLabel()].push_back(tree);
        }
      } else {
        if (!mismatchedNode) {
          mismatchedNode = r1;
        }
      }

      return mismatchedNode;
    }

    // (expr ...) and (expr ...)
    if (r1->children.size() != r2->children.size()) {
      if (mismatchedNode == nullptr) {
        mismatchedNode = r1;
      }

      return mismatchedNode;
    }

    std::size_t n = r1->children.size();
    for (size_t i = 0; i < n; i++) {
      ParseTree* childMatch =
          matchImpl(r1->children[i], patternTree->children[i], labels);
      if (childMatch) {
        return childMatch;
      }
    }

    return mismatchedNode;
  }

  // if nodes aren't both tokens or both rule nodes, can't match
  return tree;
}

RuleTagToken* ParseTreePatternMatcher::getRuleTagToken(ParseTree* t) {
  if (t->children.size() == 1 && is<TerminalNode*>(t->children[0])) {
    TerminalNode* c = dynamic_cast<TerminalNode*>(t->children[0]);
    if (is<RuleTagToken*>(c->getSymbol())) {
      return dynamic_cast<RuleTagToken*>(c->getSymbol());
    }
  }
  return nullptr;
}

std::vector<std::unique_ptr<Token>> ParseTreePatternMatcher::tokenize(
    const std::string& pattern) {
  // split pattern into chunks: sea (raw input) and islands (<ID>, <expr>)
  std::vector<Chunk> chunks = split(pattern);

  // create token stream from text and tags
  std::vector<std::unique_ptr<Token>> tokens;
  for (auto chunk : chunks) {
    if (is<TagChunk*>(&chunk)) {
      TagChunk& tagChunk = (TagChunk&)chunk;
      // add special rule token or conjure up new token from name
      if (isupper(tagChunk.getTag()[0])) {
        size_t ttype = _parser->getTokenType(tagChunk.getTag());
        if (ttype == Token::INVALID_TYPE) {
          throw IllegalArgumentException("Unknown token " + tagChunk.getTag() +
                                         " in pattern: " + pattern);
        }
        tokens.emplace_back(new TokenTagToken(tagChunk.getTag(), (int)ttype,
                                              tagChunk.getLabel()));
      } else if (islower(tagChunk.getTag()[0])) {
        size_t ruleIndex = _parser->getRuleIndex(tagChunk.getTag());
        if (ruleIndex == INVALID_INDEX) {
          throw IllegalArgumentException("Unknown rule " + tagChunk.getTag() +
                                         " in pattern: " + pattern);
        }
        size_t ruleImaginaryTokenType =
            _parser->getATNWithBypassAlts().ruleToTokenType[ruleIndex];
        tokens.emplace_back(new RuleTagToken(
            tagChunk.getTag(), ruleImaginaryTokenType, tagChunk.getLabel()));
      } else {
        throw IllegalArgumentException("invalid tag: " + tagChunk.getTag() +
                                       " in pattern: " + pattern);
      }
    } else {
      TextChunk& textChunk = (TextChunk&)chunk;
      ANTLRInputStream input(textChunk.getText());
      _lexer->setInputStream(&input);
      std::unique_ptr<Token> t(_lexer->nextToken());
      while (t->getType() != Token::EOF) {
        tokens.push_back(std::move(t));
        t = _lexer->nextToken();
      }
      _lexer->setInputStream(nullptr);
    }
  }

  return tokens;
}

std::vector<Chunk> ParseTreePatternMatcher::split(const std::string& pattern) {
  size_t p = 0;
  size_t n = pattern.length();
  std::vector<Chunk> chunks;

  // find all start and stop indexes first, then collect
  std::vector<size_t> starts;
  std::vector<size_t> stops;
  while (p < n) {
    if (p == pattern.find(_escape + _start, p)) {
      p += _escape.length() + _start.length();
    } else if (p == pattern.find(_escape + _stop, p)) {
      p += _escape.length() + _stop.length();
    } else if (p == pattern.find(_start, p)) {
      starts.push_back(p);
      p += _start.length();
    } else if (p == pattern.find(_stop, p)) {
      stops.push_back(p);
      p += _stop.length();
    } else {
      p++;
    }
  }

  if (starts.size() > stops.size()) {
    throw IllegalArgumentException("unterminated tag in pattern: " + pattern);
  }

  if (starts.size() < stops.size()) {
    throw IllegalArgumentException("missing start tag in pattern: " + pattern);
  }

  size_t ntags = starts.size();
  for (size_t i = 0; i < ntags; i++) {
    if (starts[i] >= stops[i]) {
      throw IllegalArgumentException(
          "tag delimiters out of order in pattern: " + pattern);
    }
  }

  // collect into chunks now
  if (ntags == 0) {
    std::string text = pattern.substr(0, n);
    chunks.push_back(TextChunk(text));
  }

  if (ntags > 0 && starts[0] > 0) {  // copy text up to first tag into chunks
    std::string text = pattern.substr(0, starts[0]);
    chunks.push_back(TextChunk(text));
  }

  for (size_t i = 0; i < ntags; i++) {
    // copy inside of <tag>
    std::string tag = pattern.substr(starts[i] + _start.length(),
                                     stops[i] - (starts[i] + _start.length()));
    std::string ruleOrToken = tag;
    std::string label = "";
    size_t colon = tag.find(':');
    if (colon != std::string::npos) {
      label = tag.substr(0, colon);
      ruleOrToken = tag.substr(colon + 1, tag.length() - (colon + 1));
    }
    chunks.push_back(TagChunk(label, ruleOrToken));
    if (i + 1 < ntags) {
      // copy from end of <tag> to start of next
      std::string text =
          pattern.substr(stops[i] + _stop.length(),
                         starts[i + 1] - (stops[i] + _stop.length()));
      chunks.push_back(TextChunk(text));
    }
  }

  if (ntags > 0) {
    size_t afterLastTag = stops[ntags - 1] + _stop.length();
    if (afterLastTag < n) {  // copy text from end of last tag to end
      std::string text = pattern.substr(afterLastTag, n - afterLastTag);
      chunks.push_back(TextChunk(text));
    }
  }

  // strip out all backslashes from text chunks but not tags
  for (size_t i = 0; i < chunks.size(); i++) {
    Chunk& c = chunks[i];
    if (is<TextChunk*>(&c)) {
      TextChunk& tc = (TextChunk&)c;
      std::string unescaped = tc.getText();
      unescaped.erase(std::remove(unescaped.begin(), unescaped.end(), '\\'),
                      unescaped.end());
      if (unescaped.length() < tc.getText().length()) {
        chunks[i] = TextChunk(unescaped);
      }
    }
  }

  return chunks;
}

void ParseTreePatternMatcher::InitializeInstanceFields() {
  _start = "<";
  _stop = ">";
  _escape = "\\";
}
