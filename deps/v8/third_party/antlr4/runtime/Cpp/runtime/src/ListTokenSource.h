/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "CommonTokenFactory.h"
#include "TokenSource.h"

namespace antlr4 {

/// Provides an implementation of <seealso cref="TokenSource"/> as a wrapper
/// around a list of <seealso cref="Token"/> objects.
///
/// If the final token in the list is an <seealso cref="Token#EOF"/> token, it
/// will be used as the EOF token for every call to <seealso cref="#nextToken"/>
/// after the end of the list is reached. Otherwise, an EOF token will be
/// created.
class ANTLR4CPP_PUBLIC ListTokenSource : public TokenSource {
 protected:
  // This list will be emptied token by token as we call nextToken().
  // Token streams can be used to buffer tokens for a while.
  std::vector<std::unique_ptr<Token>> tokens;

 private:
  /// <summary>
  /// The name of the input source. If this value is {@code null}, a call to
  /// <seealso cref="#getSourceName"/> should return the source name used to
  /// create the the next token in <seealso cref="#tokens"/> (or the previous
  /// token if the end of the input has been reached).
  /// </summary>
  const std::string sourceName;

 protected:
  /// The index into <seealso cref="#tokens"/> of token to return by the next
  /// call to <seealso cref="#nextToken"/>. The end of the input is indicated by
  /// this value being greater than or equal to the number of items in <seealso
  /// cref="#tokens"/>.
  size_t i;

 private:
  /// This is the backing field for <seealso cref="#getTokenFactory"/> and
  /// <seealso cref="setTokenFactory"/>.
  Ref<TokenFactory<CommonToken>> _factory = CommonTokenFactory::DEFAULT;

 public:
  /// Constructs a new <seealso cref="ListTokenSource"/> instance from the
  /// specified collection of <seealso cref="Token"/> objects.
  ///
  /// <param name="tokens"> The collection of <seealso cref="Token"/> objects to
  /// provide as a <seealso cref="TokenSource"/>. </param> <exception
  /// cref="NullPointerException"> if {@code tokens} is {@code null}
  /// </exception>
  ListTokenSource(std::vector<std::unique_ptr<Token>> tokens);
  ListTokenSource(const ListTokenSource& other) = delete;

  ListTokenSource& operator=(const ListTokenSource& other) = delete;

  /// <summary>
  /// Constructs a new <seealso cref="ListTokenSource"/> instance from the
  /// specified collection of <seealso cref="Token"/> objects and source name.
  /// </summary>
  /// <param name="tokens"> The collection of <seealso cref="Token"/> objects to
  /// provide as a <seealso cref="TokenSource"/>. </param> <param
  /// name="sourceName"> The name of the <seealso cref="TokenSource"/>. If this
  /// value is
  /// {@code null}, <seealso cref="#getSourceName"/> will attempt to infer the
  /// name from the next <seealso cref="Token"/> (or the previous token if the
  /// end of the input has been reached).
  /// </param>
  /// <exception cref="NullPointerException"> if {@code tokens} is {@code null}
  /// </exception>
  ListTokenSource(std::vector<std::unique_ptr<Token>> tokens_,
                  const std::string& sourceName_);

  virtual size_t getCharPositionInLine() override;
  virtual std::unique_ptr<Token> nextToken() override;
  virtual size_t getLine() const override;
  virtual CharStream* getInputStream() override;
  virtual std::string getSourceName() override;

  template <typename T1>
  void setTokenFactory(TokenFactory<T1>* factory) {
    this->_factory = factory;
  }

  virtual Ref<TokenFactory<CommonToken>> getTokenFactory() override;

 private:
  void InitializeInstanceFields();
};

}  // namespace antlr4
