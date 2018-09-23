// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_D8_CONSOLE_H_
#define V8_D8_CONSOLE_H_

#include "src/base/platform/time.h"
#include "src/debug/debug-interface.h"
#include "src/debug/interface-types.h"

namespace v8 {

class D8Console : public debug::ConsoleDelegate {
 public:
  explicit D8Console(Isolate* isolate);

 private:
  void Assert(const debug::ConsoleCallArguments& args,
              const v8::debug::ConsoleContext&) override;
  void Log(const debug::ConsoleCallArguments& args,
           const v8::debug::ConsoleContext&) override;
  void Error(const debug::ConsoleCallArguments& args,
             const v8::debug::ConsoleContext&) override;
  void Warn(const debug::ConsoleCallArguments& args,
            const v8::debug::ConsoleContext&) override;
  void Info(const debug::ConsoleCallArguments& args,
            const v8::debug::ConsoleContext&) override;
  void Debug(const debug::ConsoleCallArguments& args,
             const v8::debug::ConsoleContext&) override;
  void Time(const debug::ConsoleCallArguments& args,
            const v8::debug::ConsoleContext&) override;
  void TimeEnd(const debug::ConsoleCallArguments& args,
               const v8::debug::ConsoleContext&) override;
  void TimeStamp(const debug::ConsoleCallArguments& args,
                 const v8::debug::ConsoleContext&) override;
  void Trace(const debug::ConsoleCallArguments& args,
             const v8::debug::ConsoleContext&) override;

  Isolate* isolate_;
  std::map<std::string, base::TimeTicks> timers_;
  base::TimeTicks default_timer_;
};

}  // namespace v8

#endif  // V8_D8_CONSOLE_H_
