// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
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

#ifndef V8_CPU_PROFILER_H_
#define V8_CPU_PROFILER_H_

#include "circular-queue.h"
#include "profile-generator.h"

namespace v8 {
namespace internal {


#define CODE_EVENTS_TYPE_LIST(V)                \
  V(CODE_CREATION, CodeCreateEventRecord)       \
  V(CODE_MOVE,     CodeMoveEventRecord)         \
  V(CODE_DELETE,   CodeDeleteEventRecord)       \
  V(CODE_ALIAS,    CodeAliasEventRecord)


class CodeEventRecord {
 public:
#define DECLARE_TYPE(type, ignore) type,
  enum Type {
    NONE = 0,
    CODE_EVENTS_TYPE_LIST(DECLARE_TYPE)
    NUMBER_OF_TYPES
  };
#undef DECLARE_TYPE

  Type type;
  unsigned order;
};


class CodeCreateEventRecord : public CodeEventRecord {
 public:
  Address start;
  CodeEntry* entry;
  unsigned size;

  INLINE(void UpdateCodeMap(CodeMap* code_map)) {
    code_map->AddCode(start, entry, size);
  }
};


class CodeMoveEventRecord : public CodeEventRecord {
 public:
  Address from;
  Address to;

  INLINE(void UpdateCodeMap(CodeMap* code_map)) {
    code_map->MoveCode(from, to);
  }
};


class CodeDeleteEventRecord : public CodeEventRecord {
 public:
  Address start;

  INLINE(void UpdateCodeMap(CodeMap* code_map)) {
    code_map->DeleteCode(start);
  }
};


class CodeAliasEventRecord : public CodeEventRecord {
 public:
  Address alias;
  Address start;

  INLINE(void UpdateCodeMap(CodeMap* code_map)) {
    code_map->AddAlias(alias, start);
  }
};


class TickSampleEventRecord {
 public:
  // In memory, the first machine word of a TickSampleEventRecord will be the
  // first entry of TickSample, that is -- a program counter field.
  // TickSample is put first, because 'order' can become equal to
  // SamplingCircularQueue::kClear, while program counter can't.
  TickSample sample;
  unsigned order;

#if defined(__GNUC__) && (__GNUC__ < 4)
  // Added to avoid 'all member functions in class are private' warning.
  INLINE(unsigned get_order() const) { return order; }
  // Added to avoid 'class only defines private constructors and
  // has no friends' warning.
  friend class TickSampleEventRecordFriend;
#endif
 private:
  // Disable instantiation.
  TickSampleEventRecord();

  DISALLOW_COPY_AND_ASSIGN(TickSampleEventRecord);
};


// This class implements both the profile events processor thread and
// methods called by event producers: VM and stack sampler threads.
class ProfilerEventsProcessor : public Thread {
 public:
  explicit ProfilerEventsProcessor(ProfileGenerator* generator);
  virtual ~ProfilerEventsProcessor() { }

  // Thread control.
  virtual void Run();
  inline void Stop() { running_ = false; }
  INLINE(bool running()) { return running_; }

  // Events adding methods. Called by VM threads.
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       String* name,
                       String* resource_name, int line_number,
                       Address start, unsigned size);
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       const char* name,
                       Address start, unsigned size);
  void CodeCreateEvent(Logger::LogEventsAndTags tag,
                       int args_count,
                       Address start, unsigned size);
  void CodeMoveEvent(Address from, Address to);
  void CodeDeleteEvent(Address from);
  void FunctionCreateEvent(Address alias, Address start);
  void FunctionMoveEvent(Address from, Address to);
  void FunctionDeleteEvent(Address from);

  // Tick sampler registration. Called by sampler thread or signal handler.
  inline void SetUpSamplesProducer() { ticks_buffer_.SetUpProducer(); }
  // Tick sample events are filled directly in the buffer of the circular
  // queue (because the structure is of fixed width, but usually not all
  // stack frame entries are filled.) This method returns a pointer to the
  // next record of the buffer.
  INLINE(TickSample* TickSampleEvent());
  inline void TearDownSamplesProducer() { ticks_buffer_.TearDownProducer(); }

 private:
  union CodeEventsContainer {
    CodeEventRecord generic;
#define DECLARE_CLASS(ignore, type) type type##_;
    CODE_EVENTS_TYPE_LIST(DECLARE_CLASS)
#undef DECLARE_TYPE
  };

  // Called from events processing thread (Run() method.)
  bool ProcessCodeEvent(unsigned* dequeue_order);
  bool ProcessTicks(unsigned dequeue_order);

  ProfileGenerator* generator_;
  bool running_;
  CircularQueue<CodeEventsContainer> events_buffer_;
  SamplingCircularQueue ticks_buffer_;
  unsigned enqueue_order_;
};


} }  // namespace v8::internal

#endif  // V8_CPU_PROFILER_H_
