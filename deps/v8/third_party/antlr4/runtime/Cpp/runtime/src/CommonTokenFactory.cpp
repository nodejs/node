/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#include "CharStream.h"
#include "CommonToken.h"
#include "misc/Interval.h"

#include "CommonTokenFactory.h"

using namespace antlr4;

const Ref<TokenFactory<CommonToken>> CommonTokenFactory::DEFAULT =
    std::make_shared<CommonTokenFactory>();

CommonTokenFactory::CommonTokenFactory(bool copyText_) : copyText(copyText_) {}

CommonTokenFactory::CommonTokenFactory() : CommonTokenFactory(false) {}

std::unique_ptr<CommonToken> CommonTokenFactory::create(
    std::pair<TokenSource*, CharStream*> source, size_t type,
    const std::string& text, size_t channel, size_t start, size_t stop,
    size_t line, size_t charPositionInLine) {
  std::unique_ptr<CommonToken> t(
      new CommonToken(source, type, channel, start, stop));
  t->setLine(line);
  t->setCharPositionInLine(charPositionInLine);
  if (text != "") {
    t->setText(text);
  } else if (copyText && source.second != nullptr) {
    t->setText(source.second->getText(misc::Interval(start, stop)));
  }

  return t;
}

std::unique_ptr<CommonToken> CommonTokenFactory::create(
    size_t type, const std::string& text) {
  return std::unique_ptr<CommonToken>(new CommonToken(type, text));
}
