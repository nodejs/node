// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-background-compile --expose-gc --bytecode-old-time=0

const realm = Realm.current();
const src = "(function* gen() { yield 42; })()";

// Compile and cache the script, keeping a suspended generator alive so that its
// SFI and the Script survive GC without the generator's bytecode being flushed.
const suspended_gen = Realm.eval(realm, src);
suspended_gen.next();

// Flush the top-level SFI's bytecode while retaining the generator's bytecode.
gc();

// Re-evaluating with profiling enabled hits the partial script cache and merges
// the newly compiled top-level SFI while discarding the newly compiled
// generator SFI.
console.profile();
Realm.eval(realm, src);
