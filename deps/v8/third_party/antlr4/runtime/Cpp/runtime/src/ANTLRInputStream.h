/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "CharStream.h"

namespace antlr4 {

// Vacuum all input from a stream and then treat it
// like a string. Can also pass in a string or char[] to use.
// Input is expected to be encoded in UTF-8 and converted to UTF-32 internally.
class ANTLR4CPP_PUBLIC ANTLRInputStream : public CharStream {
 protected:
  /// The data being scanned.
  // UTF-32
  UTF32String _data;

  /// 0..n-1 index into string of next char </summary>
  size_t p;

 public:
  /// What is name or source of this char stream?
  std::string name;

  ANTLRInputStream(const std::string& input = "");
  ANTLRInputStream(const char data_[], size_t numberOfActualCharsInArray);
  ANTLRInputStream(std::istream& stream);

  virtual void load(const std::string& input);
  virtual void load(std::istream& stream);

  /// Reset the stream so that it's in the same state it was
  /// when the object was created *except* the data array is not
  /// touched.
  virtual void reset();
  virtual void consume() override;
  virtual size_t LA(ssize_t i) override;
  virtual size_t LT(ssize_t i);

  /// <summary>
  /// Return the current input symbol index 0..n where n indicates the
  ///  last symbol has been read.  The index is the index of char to
  ///  be returned from LA(1).
  /// </summary>
  virtual size_t index() override;
  virtual size_t size() override;

  /// <summary>
  /// mark/release do nothing; we have entire buffer </summary>
  virtual ssize_t mark() override;
  virtual void release(ssize_t marker) override;

  /// <summary>
  /// consume() ahead until p==index; can't just set p=index as we must
  ///  update line and charPositionInLine. If we seek backwards, just set p
  /// </summary>
  virtual void seek(size_t index) override;
  virtual std::string getText(const misc::Interval& interval) override;
  virtual std::string getSourceName() const override;
  virtual std::string toString() const override;

 private:
  void InitializeInstanceFields();
};

}  // namespace antlr4
