// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_NATIVES_H_
#define V8_NATIVES_H_

#include "src/vector.h"

namespace v8 { class StartupData; }  // Forward declaration.

namespace v8 {
namespace internal {

typedef bool (*NativeSourceCallback)(Vector<const char> name,
                                     Vector<const char> source,
                                     int index);

enum NativeType {
  CORE, EXPERIMENTAL, D8, TEST
};

template <NativeType type>
class NativesCollection {
 public:
  // Number of built-in scripts.
  static int GetBuiltinsCount();
  // Number of debugger implementation scripts.
  static int GetDebuggerCount();

  // These are used to access built-in scripts.  The debugger implementation
  // scripts have an index in the interval [0, GetDebuggerCount()).  The
  // non-debugger scripts have an index in the interval [GetDebuggerCount(),
  // GetNativesCount()).
  static int GetIndex(const char* name);
  static int GetRawScriptsSize();
  static Vector<const char> GetRawScriptSource(int index);
  static Vector<const char> GetScriptName(int index);
  static Vector<const byte> GetScriptsSource();
  static void SetRawScriptsSource(Vector<const char> raw_source);
};

typedef NativesCollection<CORE> Natives;
typedef NativesCollection<EXPERIMENTAL> ExperimentalNatives;

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
// Used for reading the natives at runtime. Implementation in natives-empty.cc
void SetNativesFromFile(StartupData* natives_blob);
#endif

} }  // namespace v8::internal

#endif  // V8_NATIVES_H_
