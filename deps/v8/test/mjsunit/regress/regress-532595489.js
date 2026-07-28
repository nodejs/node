// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const re = /(?:)/;

function Species() {
  return re;
}

const pseudo_re = {
  flags: 'g',
  lastIndex: 1073741823,
  constructor: { [Symbol.species]: Species },
};

RegExp.prototype[Symbol.matchAll].call(pseudo_re, '').next();
