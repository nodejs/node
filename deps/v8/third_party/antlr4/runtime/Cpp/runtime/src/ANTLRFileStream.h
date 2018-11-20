/* Copyright (c) 2012-2017 The ANTLR Project. All rights reserved.
 * Use of this file is governed by the BSD 3-clause license that
 * can be found in the LICENSE.txt file in the project root.
 */

#pragma once

#include "ANTLRInputStream.h"

namespace antlr4 {

/// This is an ANTLRInputStream that is loaded from a file all at once
/// when you construct the object (or call load()).
// TODO: this class needs testing.
class ANTLR4CPP_PUBLIC ANTLRFileStream : public ANTLRInputStream {
 protected:
  std::string _fileName;  // UTF-8 encoded file name.

 public:
  // Assumes a file name encoded in UTF-8 and file content in the same encoding
  // (with or w/o BOM).
  ANTLRFileStream(const std::string& fileName);

  virtual void loadFromFile(const std::string& fileName);
  virtual std::string getSourceName() const override;
};

}  // namespace antlr4
