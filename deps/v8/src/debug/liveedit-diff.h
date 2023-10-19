// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_LIVEEDIT_DIFF_H_
#define V8_DEBUG_LIVEEDIT_DIFF_H_

namespace v8 {
namespace internal {

// A general-purpose comparator between 2 arrays.
class Comparator {
 public:
  // Holds 2 arrays of some elements allowing to compare any pair of
  // element from the first array and element from the second array.
  class Input {
   public:
    virtual int GetLength1() = 0;
    virtual int GetLength2() = 0;
    virtual bool Equals(int index1, int index2) = 0;

   protected:
    virtual ~Input() = default;
  };

  // Receives compare result as a series of chunks.
  class Output {
   public:
    // Puts another chunk in result list. Note that technically speaking
    // only 3 arguments actually needed with 4th being derivable.
    virtual void AddChunk(int pos1, int pos2, int len1, int len2) = 0;

   protected:
    virtual ~Output() = default;
  };

  // Finds the difference between 2 arrays of elements.
  static void CalculateDifference(Input* input, Output* result_writer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_LIVEEDIT_DIFF_H_
