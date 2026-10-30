// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/crash.h"

#include <cxxabi.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string_view>

#include "experimental/redline/base/log.h"

namespace redline {

namespace {

uint64_t Fnv1a(std::string_view text) {
  uint64_t hash = 0xcbf29ce484222325ull;
  for (char ch : text) {
    const unsigned char c = static_cast<unsigned char>(ch);
    hash ^= c;
    hash *= 0x100000001b3ull;
  }
  return hash;
}

std::string_view Trim(std::string_view s) {
  while (!s.empty() && isspace(static_cast<unsigned char>(s.front()))) {
    s.remove_prefix(1);
  }
  while (!s.empty() && isspace(static_cast<unsigned char>(s.back()))) {
    s.remove_suffix(1);
  }
  return s;
}

std::vector<std::string_view> Lines(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  while (start < text.size()) {
    size_t nl = text.find('\n', start);
    if (nl == std::string_view::npos) {
      nl = text.size();
    }
    lines.push_back(Trim(text.substr(start, nl - start)));
    start = nl + 1;
  }
  return lines;
}

std::string Basename(std::string_view path) {
  size_t slash = path.find_last_of('/');
  return std::string(slash == std::string_view::npos ? path
                                                     : path.substr(slash + 1));
}

std::string Truncated(std::string_view text, size_t limit) {
  if (text.size() <= limit) {
    return std::string(text);
  }
  return std::string(text.substr(0, limit)) + "...";
}

// Frames of the crash machinery itself, libc and the sanitizer runtime are
// not part of the signature.
bool IsNoiseFrame(const std::string& function, std::string_view module) {
  if (function.empty()) {
    return true;
  }
  if (module.find("libc.so") != std::string_view::npos ||
      module.find("libpthread") != std::string_view::npos ||
      module.find("ld-linux") != std::string_view::npos) {
    return true;
  }
  static const char* const kPrefixes[] = {
      "v8::base::debug::",
      "v8::base::OS::Abort",
      "v8::base::OS::DebugBreak",
      "V8_Fatal",
      "v8::base::FatalOOM",
      "v8::base::(anonymous namespace)::",
      "v8::V8::FatalProcessOutOfMemory",
      "abort",
      "raise",
      "gsignal",
      "__GI_",
      "__libc_",
      "__restore_rt",
      "__asan",
      "__interceptor",
      "__sanitizer",
      "_start",
      "__libc_start",
  };
  for (const char* prefix : kPrefixes) {
    if (function.compare(0, strlen(prefix), prefix) == 0) {
      return true;
    }
  }
  // The DCHECK/CHECK failure helpers between V8_Fatal and the real frame.
  if (function.starts_with("v8::base::CheckFailed") ||
      function.starts_with("v8::base::internal::PrintCheckOperand") ||
      function.starts_with("v8::base::internal::MakeCheckOpString")) {
    return true;
  }
  return false;
}

// Parses one line of a glibc/V8 backtrace:
// "<module>(<symbol>+0x<off>)[0x<addr>]" or "<module>(<symbol>+0x<off>)
// [0x<addr>]". Returns false if the line is not a frame. `symbol` may be empty
// (no symbol for that address).
bool ParseBacktraceFrame(std::string_view line, std::string* module,
                         std::string* symbol) {
  size_t open = line.find('(');
  if (open == std::string_view::npos) {
    return false;
  }
  size_t bracket = line.rfind('[');
  if (bracket == std::string_view::npos || bracket < open) {
    return false;
  }
  size_t close = line.rfind(')', bracket);
  if (close == std::string_view::npos || close < open) {
    return false;
  }
  std::string_view inner = line.substr(open + 1, close - open - 1);
  size_t plus = inner.rfind("+0x");
  if (plus == std::string_view::npos) {
    return false;
  }
  *module = std::string(line.substr(0, open));
  *symbol = std::string(inner.substr(0, plus));
  return true;
}

// Parses a sanitizer frame: "#3 0x7f... in v8::internal::Foo(...)
// file.cc:12:3".
bool ParseSanitizerFrame(std::string_view line, std::string* symbol) {
  if (line.empty() || line[0] != '#') {
    return false;
  }
  size_t in = line.find(" in ");
  if (in == std::string_view::npos) {
    return false;
  }
  std::string_view rest = line.substr(in + 4);
  // The function name ends at the parameter list or at the file position.
  size_t end = 0;
  int depth = 0;
  for (; end < rest.size(); end++) {
    char c = rest[end];
    if (c == '(') {
      depth++;
    }
    if (c == ')') {
      depth--;
    }
    if (c == ' ' && depth <= 0) {
      break;
    }
  }
  *symbol = std::string(rest.substr(0, end));
  return !symbol->empty();
}

}  // namespace

std::string ShortFunctionName(const std::string& symbol) {
  std::string name = symbol;
  if (name.starts_with("_Z")) {
    int status = 0;
    char* demangled =
        abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
      name = demangled;
    }
    free(demangled);
  }
  // Strip the parameter list (and anything after it), but keep "operator()".
  size_t paren = name.find('(');
  if (paren != std::string::npos) {
    if (name.compare(paren, 10, "(anonymous") == 0) {
      size_t next = name.find('(', paren + 1);
      paren = next;
    } else if (paren >= 8 && name.compare(paren - 8, 8, "operator") == 0) {
      size_t next = name.find('(', paren + 2);
      paren = next;
    }
    if (paren != std::string::npos) {
      name.erase(paren);
    }
  }
  // Drop a leading return type ("void v8::internal::Foo<int>" ->
  // "v8::internal::Foo<int>"): the last space outside of template arguments
  // separates the two, except for "operator new"/"operator delete".
  int depth = 0;
  size_t last_space = std::string::npos;
  for (size_t i = 0; i < name.size(); i++) {
    if (name[i] == '<') {
      depth++;
    }
    if (name[i] == '>') {
      depth--;
    }
    if (name[i] == ' ' && depth == 0) {
      last_space = i;
    }
  }
  if (last_space != std::string::npos &&
      name.find("operator ") == std::string::npos) {
    name.erase(0, last_space + 1);
  }
  return name;
}

std::vector<std::string> CrashFrames(const std::string& stderr_text) {
  std::vector<std::string> frames;
  std::string module, symbol;
  for (std::string_view line : Lines(stderr_text)) {
    if (ParseBacktraceFrame(line, &module, &symbol)) {
      std::string function = ShortFunctionName(symbol);
      if (IsNoiseFrame(function, module)) {
        continue;
      }
      frames.push_back(function);
    } else if (ParseSanitizerFrame(line, &symbol)) {
      std::string function = ShortFunctionName(symbol);
      if (IsNoiseFrame(function, "")) {
        continue;
      }
      frames.push_back(function);
    }
  }
  return frames;
}

std::vector<std::string> RecursionCycle(
    const std::vector<std::string>& frames) {
  std::map<std::string, size_t> counts;
  for (const std::string& frame : frames) {
    counts[frame]++;
  }
  std::vector<std::string> cycle;
  size_t covered = 0;
  for (const auto& [function, count] : counts)  // sorted by name
  {
    if (count >= kMinRecursionRepeats) {
      cycle.push_back(function);
      covered += count;
    }
  }
  // The recursion has to dominate the (length-limited) trace; a function
  // that merely shows up a few times in a long trace is no recursion.
  if (cycle.empty() || covered * 2 < frames.size()) {
    return {};
  }
  if (cycle.size() > kMaxRecursionCycleFunctions) {
    cycle.resize(kMaxRecursionCycleFunctions);
  }
  return cycle;
}

// static
CrashSignature CrashSignature::FromExecution(const Execution& execution) {
  return FromOutput(execution.signal, execution.exit_code,
                    execution.stderr_output);
}

// static
CrashSignature CrashSignature::FromOutput(int signal, int exit_code,
                                          const std::string& stderr_text) {
  CrashSignature signature;
  std::vector<std::string_view> lines = Lines(stderr_text);
  std::string text;

  // 1. V8 fatal errors and out-of-memory.
  for (size_t i = 0; i < lines.size() && text.empty(); i++) {
    std::string_view line = lines[i];
    constexpr std::string_view kFatal = "Fatal error in ";
    constexpr std::string_view kOom = "Fatal process out of memory: ";
    constexpr std::string_view kJsOom = "Fatal JavaScript out of memory: ";
    constexpr std::string_view kJsInvalidSize =
        "Fatal JavaScript invalid size error";
    if (size_t oom_at = line.find(kOom); oom_at != std::string_view::npos) {
      text = "oom " + Truncated(Trim(line.substr(oom_at + kOom.size())), 60);
    } else if (size_t js_oom_at = line.find(kJsOom);
               js_oom_at != std::string_view::npos) {
      text =
          "oom " + Truncated(Trim(line.substr(js_oom_at + kJsOom.size())), 60);
    } else if (line.find(kJsInvalidSize) != std::string_view::npos) {
      text = "oom invalid size";
    } else if (size_t fatal_at = line.find(kFatal);
               fatal_at != std::string_view::npos) {
      std::string_view rest = line.substr(fatal_at + kFatal.size());
      size_t comma = rest.find(", line ");
      std::string location;
      if (comma != std::string_view::npos) {
        location = Basename(rest.substr(0, comma)) + ":" +
                   std::string(Trim(rest.substr(comma + 7)));
      } else {
        location = std::string(Trim(rest));
      }
      text = "fatal " + location;
      // The check that failed usually follows within a few lines.
      for (size_t j = i + 1; j < lines.size() && j < i + 6; j++) {
        std::string_view next = lines[j];
        constexpr std::string_view kCheck = "heck failed: ";
        size_t at = next.find(kCheck);
        if (at != std::string_view::npos) {
          text += " " + Truncated(Trim(next.substr(at + kCheck.size())), 80);
          break;
        }
        if (next.find("Sandbox violation") != std::string_view::npos ||
            next.find("sandbox violation") != std::string_view::npos) {
          text += " sandbox violation";
          break;
        }
      }
    }
  }

  // 2. Sanitizer reports.
  if (text.empty()) {
    for (std::string_view line : lines) {
      size_t pos = line.find("ERROR: ");
      if (pos == std::string_view::npos) {
        continue;
      }
      std::string_view rest = line.substr(pos + 7);
      size_t colon = rest.find(": ");
      if (colon == std::string_view::npos) {
        continue;
      }
      std::string_view tool = rest.substr(0, colon);
      if (tool.find("Sanitizer") == std::string_view::npos) {
        continue;
      }
      std::string_view kind = rest.substr(colon + 2);
      size_t space = kind.find(' ');
      if (space != std::string_view::npos) {
        kind = kind.substr(0, space);
      }
      text = "asan " + std::string(kind);
      std::vector<std::string> frames = CrashFrames(stderr_text);
      if (!frames.empty()) {
        text += " " + frames[0];
      }
      break;
    }
  }

  // 3. Signals with a stack trace.
  if (text.empty()) {
    std::string head = StringPrintf("sig%d", signal);
    for (std::string_view line : lines) {
      constexpr std::string_view kSignal = "Received signal ";
      size_t pos = line.find(kSignal);
      if (pos == std::string_view::npos) {
        continue;
      }
      std::string_view rest = line.substr(pos + kSignal.size());
      // "11 SEGV_MAPERR 000000000000" -> "sig11 SEGV_MAPERR" (the address is
      // not part of the signature).
      size_t space = rest.find(' ');
      if (space != std::string_view::npos) {
        std::string_view code = rest.substr(space + 1);
        size_t end = code.find(' ');
        if (end != std::string_view::npos) {
          code = code.substr(0, end);
        }
        head += " " + std::string(code);
      }
      break;
    }
    std::vector<std::string> frames = CrashFrames(stderr_text);
    text = head;
    std::vector<std::string> cycle = RecursionCycle(frames);
    if (!cycle.empty()) {
      text += " stack-overflow";
      for (size_t i = 0; i < cycle.size(); i++) {
        text += (i == 0 ? " " : " | ") + cycle[i];
      }
    }
    for (size_t i = 0; cycle.empty() && i < frames.size() && i < 3; i++) {
      text += (i == 0 ? " " : " < ") + frames[i];
    }
    if (signal == 0 && frames.empty()) {
      text = StringPrintf("exit%d", exit_code);
    }
  }

  signature.text = text;
  signature.hash = Fnv1a(text);
  return signature;
}

}  // namespace redline
