// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --super-ic --sparkplug --no-always-sparkplug

export let exported = 17;
import imported from 'test-baseline-module-helper.mjs';

function run(f, ...args) {
  try { f(...args); } catch (e) {}
  %CompileBaseline(f);
  return f(...args);
}

function construct(f, ...args) {
  try { new f(...args); } catch (e) {}
  %CompileBaseline(f);
  return new f(...args);
}

assertEquals(17, run((o)=>{ return exported; }));
assertEquals(12, run((o)=>{ return imported; }));
assertEquals(20, run((o)=>{ exported = 20; return exported; }));
