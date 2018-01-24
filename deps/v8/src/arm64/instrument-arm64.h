// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_INSTRUMENT_ARM64_H_
#define V8_ARM64_INSTRUMENT_ARM64_H_

#include "src/globals.h"
#include "src/utils.h"

#include "src/arm64/constants-arm64.h"
#include "src/arm64/decoder-arm64.h"

namespace v8 {
namespace internal {

const int kCounterNameMaxLength = 256;
const uint64_t kDefaultInstrumentationSamplingPeriod = 1 << 22;


enum InstrumentState {
  InstrumentStateDisable = 0,
  InstrumentStateEnable = 1
};


enum CounterType {
  Gauge = 0,      // Gauge counters reset themselves after reading.
  Cumulative = 1  // Cumulative counters keep their value after reading.
};


class Counter {
 public:
  explicit Counter(const char* name, CounterType type = Gauge);

  void Increment();
  void Enable();
  void Disable();
  bool IsEnabled();
  uint64_t count();
  const char* name();
  CounterType type();

 private:
  char name_[kCounterNameMaxLength];
  uint64_t count_;
  bool enabled_;
  CounterType type_;
};


class Instrument: public DecoderVisitor {
 public:
  explicit Instrument(
      const char* datafile = nullptr,
      uint64_t sample_period = kDefaultInstrumentationSamplingPeriod);
  ~Instrument();

  // Declare all Visitor functions.
  #define DECLARE(A) void Visit##A(Instruction* instr);
  VISITOR_LIST(DECLARE)
  #undef DECLARE

 private:
  void Update();
  void Enable();
  void Disable();
  void DumpCounters();
  void DumpCounterNames();
  void DumpEventMarker(unsigned marker);
  void HandleInstrumentationEvent(unsigned event);
  Counter* GetCounter(const char* name);

  void InstrumentLoadStore(Instruction* instr);
  void InstrumentLoadStorePair(Instruction* instr);

  std::list<Counter*> counters_;

  FILE *output_stream_;
  uint64_t sample_period_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ARM64_INSTRUMENT_ARM64_H_
