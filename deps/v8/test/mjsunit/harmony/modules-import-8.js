// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-dynamic-import

var ran = false;

var x = {
  get toString() { return undefined; }
};
import(x);

var x = {
  toString() {
    throw new Error('42 is the answer');
  }
};
import(x);

var x = {
  get toString() {
    throw new Error('42 is the answer');
  }
};
import(x);

async function test1() {
  try {
    let x = {
      toString() {
        throw new Error('42 is the answer');
      }
    };

    let namespace = await import(x);
    %AbortJS('failure: this should throw');
  } catch(e) {
    assertEquals(e.message, '42 is the answer');
    ran = true;
  }
}

test1();

%PerformMicrotaskCheckpoint();

assertTrue(ran);

ran = false;
async function test2() {
  try {
    let x = {
      get toString() {
        throw new Error('42 is the answer');
      }
    };

    let namespace = await import(x);
    %AbortJS('failure: this should throw');
  } catch(e) {
    assertEquals(e.message, '42 is the answer');
    ran = true;
  }
}

test2();

%PerformMicrotaskCheckpoint();

assertTrue(ran);

ran = false;
async function test3() {
  try {
    let x = {
      get toString() { return undefined; }
    };
    let namespace = await import(x);
    %AbortJS('failure: this should throw');
  } catch(e) {
    assertEquals(e.message, 'Cannot convert object to primitive value');
    ran = true;
  }
}

test3();

%PerformMicrotaskCheckpoint();

assertTrue(ran);
