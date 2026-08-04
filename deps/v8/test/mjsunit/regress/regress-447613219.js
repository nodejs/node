// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stack-size=64

function id(x) {
  return x;
}

function main() {
  try {
    var s = id('');
  } catch (e) {
  }
  try {
    s.replace(/bar\d\d/);
  } catch (e) {
  }
  try {
    main();
  } catch (e) {
  }
  try {
    /bar\d\d/g;
  } catch (e) {
  }
  try {
    helper();
  } catch (e) {
  }
}

function helper() {
  try {
    var s = id('');
  } catch (e) {
  }
  try {
    var r = /bar\d\d/;
  } catch (e) {
  }
  try {
    s.replace(r, id('--$&--'));
  } catch (e) {
  }
  try {
    s += '\u1200';
  } catch (e) {
  }
  try {
    s.replace(r);
  } catch (e) {
  }
}

try {
  main();
} catch (e) {
}
