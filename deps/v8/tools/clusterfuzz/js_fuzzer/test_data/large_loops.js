// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For-loop examples that we approximate as either large or small.

// Large loops that we'll determine as large:
for (const a = 0; a < 10000; a++) {
  console.log("Large1");
}

for (const a = 0; a < 1e3; a++) {
  console.log("Large2");
}

for (const a = -0.01e7; a < 0.01e7; a++) {
  console.log("Large3");
}

for (const a = 0; 10000 >= a; a++) {
  console.log("Large4");
}

for (const a = 1000; a != 0; a--) {
  console.log("Large5");
}

for (const a = 1e3; 0 != a; a--) {
  console.log("Large6");
}

for (const a = 29303.34; 124.2 != a; a -= 0.7) {
  console.log("Large7");
}

for (const a = 0n; a < 3000000000000n; a++) {
  console.log("Large8");
}

// Test that we internally don't make a mixed bigint/non-bigint computation.
for (const a = 0; a < 30000000000000000000000000n; a++) {
  console.log("Large10");
}

// Not really large, but we still approximate it as large:
for (const a = 0; a < 10000; a += 1000) {
  console.log("Large11");
}

// Small loops that we'll determine as small:
for (const a = 0; a < 10; a++) {
  console.log("Small1");
}

for (const a = 0; a < 999; a++) {
  console.log("Small2");
}

for (const a = 3; 0 != a; a--) {
  console.log("Small3");
}

for (const a = 10000; a > 9100; a--) {
  console.log("Small4");
}

// Large loops that we can't determine, so we approximate them
// as small.
const b = 10000;
for (const a = b; a > 0; a--) {
  console.log("Small5");
}

for (const a = 10000, b = 10000; a > 0; a--) {
  console.log("Small6");
}

const c = 0;
for (const a = 10000; a > c; a--) {
  console.log("Small7");
}

for (const a = 10000; a > 0 && other; a--) {
  console.log("Small8");
}

// Types and structures that we can't determine, hence small.
for (var str = "start"; str < "end"; str += "inc") {
  console.log("Small9");
}

for (let a = []; a < [0, 1, 2]; a += [0]) {
  console.log("Small10");
}

for (let {a} = 123; a < 10000; a++) {
  console.log("Small11");
}

for (let a = !1; a < 10000; a++) {
  console.log("Small12");
}

// Other stuff for code coverage.
let d = 0
for (; d < 10000; d++) {
  console.log("Small13");
}

for (let a = 0;;) {
  console.log("Small14");
}

for (;;) {
  console.log("Small15");
}

for (call();call();call()) {
  console.log("Small16");
}
