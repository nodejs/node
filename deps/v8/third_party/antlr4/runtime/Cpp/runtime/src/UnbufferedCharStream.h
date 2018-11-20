/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "CharStream.h"

namespace antlr4 {

/// Do not buffer up the entire char stream. It does keep a small buffer
/// for efficiency and also buffers while a mark exists (set by the
/// lookahead prediction in parser). "Unbuffered" here refers to fact
/// that it doesn't buffer all data, not that's it's on demand loading of char.
class ANTLR4CPP_PUBLIC UnbufferedCharStream : public CharStream {
 public:
  /// The name or source of this char stream.
  std::string name;

  UnbufferedCharStream(std::wistream& input);

  virtual void consume() override;
  virtual size_t LA(ssize_t i) override;

  /// <summary>
  /// Return a marker that we can release later.
  /// <p/>
  /// The specific marker value used for this class allows for some level of
  /// protection against misuse where {@code seek()} is called on a mark or
  /// {@code release()} is called in the wrong order.
  /// </summary>
  virtual ssize_t mark() override;

  /// <summary>
  /// Decrement number of markers, resetting buffer if we hit 0. </summary>
  /// <param name="marker"> </param>
  virtual void release(ssize_t marker) override;
  virtual size_t index() override;

  /// <summary>
  /// Seek to absolute character index, which might not be in the current
  ///  sliding window.  Move {@code p} to {@code index-bufferStartIndex}.
  /// </summary>
  virtual void seek(size_t index) override;
  virtual size_t size() override;
  virtual std::string getSourceName() const override;
  virtual std::string getText(const misc::Interval& interval) override;

 protected:
/// A moving window buffer of the data being scanned. While there's a marker,
/// we keep adding to buffer. Otherwise, <seealso cref="#consume consume()"/>
/// resets so we start filling at index 0 again.
// UTF-32 encoded.
#if defined(_MSC_VER) && _MSC_VER == 1900
  i32string _data;  // Custom type for VS 2015.
  typedef __int32 storage_type;
#else
  std::u32string _data;
  typedef char32_t storage_type;
#endif

  /// <summary>
  /// 0..n-1 index into <seealso cref="#data data"/> of next character.
  /// <p/>
  /// The {@code LA(1)} character is {@code data[p]}. If {@code p == n}, we are
  /// out of buffered characters.
  /// </summary>
  size_t _p;

  /// <summary>
  /// Count up with <seealso cref="#mark mark()"/> and down with
  /// <seealso cref="#release release()"/>. When we {@code release()} the last
  /// mark,
  /// {@code numMarkers} reaches 0 and we reset the buffer. Copy
  /// {@code data[p]..data[n-1]} to {@code data[0]..data[(n-1)-p]}.
  /// </summary>
  size_t _numMarkers;

  /// This is the {@code LA(-1)} character for the current position.
  size_t _lastChar;  // UTF-32

  /// <summary>
  /// When {@code numMarkers > 0}, this is the {@code LA(-1)} character for the
  /// first character in <seealso cref="#data data"/>. Otherwise, this is
  /// unspecified.
  /// </summary>
  size_t _lastCharBufferStart;  // UTF-32

  /// <summary>
  /// Absolute character index. It's the index of the character about to be
  /// read via {@code LA(1)}. Goes from 0 to the number of characters in the
  /// entire stream, although the stream size is unknown before the end is
  /// reached.
  /// </summary>
  size_t _currentCharIndex;

  std::wistream& _input;

  /// <summary>
  /// Make sure we have 'want' elements from current position <seealso cref="#p
  /// p"/>. Last valid {@code p} index is {@code data.length-1}. {@code
  /// p+need-1} is the char index 'need' elements ahead. If we need 1 element,
  /// {@code (p+1-1)==p} must be less than {@code data.length}.
  /// </summary>
  virtual void sync(size_t want);

  /// <summary>
  /// Add {@code n} characters to the buffer. Returns the number of characters
  /// actually added to the buffer. If the return value is less than {@code n},
  /// then EOF was reached before {@code n} characters could be added.
  /// </summary>
  virtual size_t fill(size_t n);

  /// Override to provide different source of characters than
  /// <seealso cref="#input input"/>.
  virtual char32_t nextChar();
  virtual void add(char32_t c);
  size_t getBufferStartIndex() const;

 private:
  void InitializeInstanceFields();
};

}  // namespace antlr4
