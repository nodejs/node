// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TRIG_TABLE_H_
#define V8_TRIG_TABLE_H_


namespace v8 {
namespace internal {

class TrigonometricLookupTable : public AllStatic {
 public:
  // Casting away const-ness to use as argument for typed array constructor.
  static void* sin_table() {
    return const_cast<double*>(&kSinTable[0]);
  }

  static void* cos_x_interval_table() {
    return const_cast<double*>(&kCosXIntervalTable[0]);
  }

  static double samples_over_pi_half() { return kSamplesOverPiHalf; }
  static int samples() { return kSamples; }
  static int table_num_bytes() { return kTableSize * sizeof(*kSinTable); }
  static int table_size() { return kTableSize; }

 private:
  static const double kSinTable[];
  static const double kCosXIntervalTable[];
  static const int kSamples;
  static const int kTableSize;
  static const double kSamplesOverPiHalf;
};

} }  // namespace v8::internal

#endif  // V8_TRIG_TABLE_H_
