// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

// The infrastructure used for (localized) message reporting in V8.
//
// Note: there's a big unresolved issue about ownership of the data
// structures used by this framework.

#ifndef V8_MESSAGES_H_
#define V8_MESSAGES_H_

#include "handles-inl.h"

// Forward declaration of MessageLocation.
namespace v8 {
namespace internal {
class MessageLocation;
} }  // namespace v8::internal


class V8Message {
 public:
  V8Message(char* type,
            v8::internal::Handle<v8::internal::JSArray> args,
            const v8::internal::MessageLocation* loc) :
      type_(type), args_(args), loc_(loc) { }
  char* type() const { return type_; }
  v8::internal::Handle<v8::internal::JSArray> args() const { return args_; }
  const v8::internal::MessageLocation* loc() const { return loc_; }
 private:
  char* type_;
  v8::internal::Handle<v8::internal::JSArray> const args_;
  const v8::internal::MessageLocation* loc_;
};


namespace v8 {
namespace internal {

struct Language;
class SourceInfo;

class MessageLocation {
 public:
  MessageLocation(Handle<Script> script,
                  int start_pos,
                  int end_pos)
      : script_(script),
        start_pos_(start_pos),
        end_pos_(end_pos) { }
  MessageLocation() : start_pos_(-1), end_pos_(-1) { }

  Handle<Script> script() const { return script_; }
  int start_pos() const { return start_pos_; }
  int end_pos() const { return end_pos_; }

 private:
  Handle<Script> script_;
  int start_pos_;
  int end_pos_;
};


// A message handler is a convenience interface for accessing the list
// of message listeners registered in an environment
class MessageHandler {
 public:
  // Returns a message object for the API to use.
  static Handle<JSMessageObject> MakeMessageObject(
      Isolate* isolate,
      const char* type,
      MessageLocation* loc,
      Vector< Handle<Object> > args,
      Handle<JSArray> stack_frames);

  // Report a formatted message (needs JS allocation).
  static void ReportMessage(Isolate* isolate,
                            MessageLocation* loc,
                            Handle<Object> message);

  static void DefaultMessageReport(Isolate* isolate,
                                   const MessageLocation* loc,
                                   Handle<Object> message_obj);
  static Handle<String> GetMessage(Isolate* isolate, Handle<Object> data);
  static SmartArrayPointer<char> GetLocalizedMessage(Isolate* isolate,
                                                     Handle<Object> data);
};

} }  // namespace v8::internal

#endif  // V8_MESSAGES_H_
