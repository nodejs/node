// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <fstream>
#include <iostream>
#include "benchmark/benchmark.h"
#include "gogo/cpp_no_group/benchmarks.pb.h"
#include "gogo/cpp_no_group/datasets/google_message1/proto2/benchmark_message1_proto2.pb.h"
#include "gogo/cpp_no_group/datasets/google_message1/proto3/benchmark_message1_proto3.pb.h"
#include "gogo/cpp_no_group/datasets/google_message2/benchmark_message2.pb.h"
#include "gogo/cpp_no_group/datasets/google_message3/benchmark_message3.pb.h"
#include "gogo/cpp_no_group/datasets/google_message4/benchmark_message4.pb.h"


#define PREFIX "dataset."
#define SUFFIX ".pb"

using benchmarks::BenchmarkDataset;
using google::protobuf::Arena;
using google::protobuf::Descriptor;
using google::protobuf::DescriptorPool;
using google::protobuf::Message;
using google::protobuf::MessageFactory;

class Fixture : public benchmark::Fixture {
 public:
  Fixture(const BenchmarkDataset& dataset, const std::string& suffix) {
    for (int i = 0; i < dataset.payload_size(); i++) {
      payloads_.push_back(dataset.payload(i));
    }

    const Descriptor* d =
        DescriptorPool::generated_pool()->FindMessageTypeByName(
            dataset.message_name());

    if (!d) {
      std::cerr << "Couldn't find message named '" << dataset.message_name()
                << "\n";
    }

    prototype_ = MessageFactory::generated_factory()->GetPrototype(d);
    SetName((dataset.name() + suffix).c_str());
  }

 protected:
  std::vector<std::string> payloads_;
  const Message* prototype_;
};

class WrappingCounter {
 public:
  WrappingCounter(size_t limit) : value_(0), limit_(limit) {}

  size_t Next() {
    size_t ret = value_;
    if (++value_ == limit_) {
      value_ = 0;
    }
    return ret;
  }

 private:
  size_t value_;
  size_t limit_;
};

template <class T>
class ParseNewFixture : public Fixture {
 public:
  ParseNewFixture(const BenchmarkDataset& dataset)
      : Fixture(dataset, "_parse_new") {}

  virtual void BenchmarkCase(benchmark::State& state) {
    WrappingCounter i(payloads_.size());
    size_t total = 0;

    while (state.KeepRunning()) {
      T m;
      const std::string& payload = payloads_[i.Next()];
      total += payload.size();
      m.ParseFromString(payload);
    }

    state.SetBytesProcessed(total);
  }
};

template <class T>
class ParseNewArenaFixture : public Fixture {
 public:
  ParseNewArenaFixture(const BenchmarkDataset& dataset)
      : Fixture(dataset, "_parse_newarena") {}

  virtual void BenchmarkCase(benchmark::State& state) {
    WrappingCounter i(payloads_.size());
    size_t total = 0;
    Arena arena;

    while (state.KeepRunning()) {
      arena.Reset();
      Message* m = Arena::CreateMessage<T>(&arena);
      const std::string& payload = payloads_[i.Next()];
      total += payload.size();
      m->ParseFromString(payload);
    }

    state.SetBytesProcessed(total);
  }
};

template <class T>
class ParseReuseFixture : public Fixture {
 public:
  ParseReuseFixture(const BenchmarkDataset& dataset)
      : Fixture(dataset, "_parse_reuse") {}

  virtual void BenchmarkCase(benchmark::State& state) {
    T m;
    WrappingCounter i(payloads_.size());
    size_t total = 0;

    while (state.KeepRunning()) {
      const std::string& payload = payloads_[i.Next()];
      total += payload.size();
      m.ParseFromString(payload);
    }

    state.SetBytesProcessed(total);
  }
};

template <class T>
class SerializeFixture : public Fixture {
 public:
  SerializeFixture(const BenchmarkDataset& dataset)
      : Fixture(dataset, "_serialize") {
    for (size_t i = 0; i < payloads_.size(); i++) {
      message_.push_back(new T);
      message_.back()->ParseFromString(payloads_[i]);
    }
  }

  ~SerializeFixture() {
    for (size_t i = 0; i < message_.size(); i++) {
      delete message_[i];
    }
  }

  virtual void BenchmarkCase(benchmark::State& state) {
    size_t total = 0;
    std::string str;
    WrappingCounter i(payloads_.size());

    while (state.KeepRunning()) {
      str.clear();
      message_[i.Next()]->SerializeToString(&str);
      total += str.size();
    }

    state.SetBytesProcessed(total);
  }

 private:
  std::vector<T*> message_;
};

std::string ReadFile(const std::string& name) {
  std::ifstream file(name.c_str());
  GOOGLE_CHECK(file.is_open()) << "Couldn't find file '" << name <<
                                  "', please make sure you are running "
                                  "this command from the benchmarks/ "
                                  "directory.\n";
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

template <class T>
void RegisterBenchmarksForType(const BenchmarkDataset& dataset) {
  ::benchmark::internal::RegisterBenchmarkInternal(
      new ParseNewFixture<T>(dataset));
  ::benchmark::internal::RegisterBenchmarkInternal(
      new ParseReuseFixture<T>(dataset));
  ::benchmark::internal::RegisterBenchmarkInternal(
      new ParseNewArenaFixture<T>(dataset));
  ::benchmark::internal::RegisterBenchmarkInternal(
      new SerializeFixture<T>(dataset));
}

void RegisterBenchmarks(const std::string& dataset_bytes) {
  BenchmarkDataset dataset;
  GOOGLE_CHECK(dataset.ParseFromString(dataset_bytes));

  if (dataset.message_name() == "benchmarks.proto3.GoogleMessage1") {
    RegisterBenchmarksForType<benchmarks::proto3::GoogleMessage1>(dataset);
  } else if (dataset.message_name() == "benchmarks.proto2.GoogleMessage1") {
    RegisterBenchmarksForType<benchmarks::proto2::GoogleMessage1>(dataset);
  } else if (dataset.message_name() == "benchmarks.proto2.GoogleMessage2") {
    RegisterBenchmarksForType<benchmarks::proto2::GoogleMessage2>(dataset);
  } else if (dataset.message_name() ==
      "benchmarks.google_message3.GoogleMessage3") {
    RegisterBenchmarksForType
    <benchmarks::google_message3::GoogleMessage3>(dataset);
  } else if (dataset.message_name() ==
      "benchmarks.google_message4.GoogleMessage4") {
    RegisterBenchmarksForType
    <benchmarks::google_message4::GoogleMessage4>(dataset);
  } else {
    std::cerr << "Unknown message type: " << dataset.message_name();
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  ::benchmark::Initialize(&argc, argv);
  if (argc == 1) {
    std::cerr << "Usage: ./cpp-benchmark <input data>" << std::endl;
    std::cerr << "input data is in the format of \"benchmarks.proto\""
        << std::endl;
    return 1;
  } else {
    for (int i = 1; i < argc; i++) {
      RegisterBenchmarks(ReadFile(argv[i]));
    }
  }

  ::benchmark::RunSpecifiedBenchmarks();
}
