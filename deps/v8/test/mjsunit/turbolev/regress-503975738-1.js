// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function makeBox(k, x) {
  return { k, x };
}

function makeThinAaaa() {
  let table = { aaaa : 1 };
  const s = String.fromCharCode(97, 97, 97, 97);
  table[s];
  return s;
}

function trigger(box, w) {
  const s = box.k;
  const pre = s.length;
  const t = w.substring(0,2);
  s === "aaaa";
  box.x = s;
  return [pre, t];
}

const warmBoxes = [makeBox("aaaa", "x0"), makeBox("bbbb", "x1")];
const box = makeBox("cccc", "x2");

// Relax the shared field metadata so Turbolev keeps the optimized code alive
// when we later swap in a ThinString.
box.k = "ffff";

%PrepareFunctionForOptimization(trigger);
trigger(warmBoxes[0], "zzzz");

%OptimizeFunctionOnNextCall(trigger);
trigger(warmBoxes[0], "zzzz");

const thin = makeThinAaaa();
box.k = thin;

%SimulateNewspaceFull();

trigger(box, "yyyy");
assertEquals("aaaa", box.x);
