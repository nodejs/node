// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";


// Data property last.

{
  const o = {
    get foo() { return 666 },
    foo: 42,
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').value);
}

{
  const o = {
    set foo(_) { },
    foo: 42,
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').value);
}

{
  const o = {
    get foo() { return 666 },
    set foo(_) { },
    foo: 42,
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').value);
}

{
  const o = {
    get foo() { return 666 },
    set ['foo'.slice()](_) { },
    foo: 42,
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').value);
}

{
  const o = {
    get ['foo'.slice()]() { return 666 },
    set ['foo'.slice()](_) { },
    foo: 42,
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').value);
}


// Data property first.

{
  const o = {
    foo: 666,
    get foo() { return 42 },
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').get());
}

{
  const o = {
    foo: 666,
    set foo(_) { },
  };
  assertEquals(undefined, Object.getOwnPropertyDescriptor(o, 'foo').get);
  assertEquals(undefined, Object.getOwnPropertyDescriptor(o, 'foo').value);
}

{
  const o = {
    foo: 666,
    get foo() { return 42 },
    set foo(_) { },
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').get());
}

{
  const o = {
    foo: 666,
    get ['foo'.slice()]() { return 42 },
    set foo(_) { },
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').get());
}

{
  const o = {
    foo: 666,
    get ['foo'.slice()]() { return 42 },
    set ['foo'](_) { },
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').get());
}


// Data property in the middle.

{
  const o = {
    get foo() { return 42 },
    foo: 666,
    set foo(_) { },
  };
  assertEquals(undefined, Object.getOwnPropertyDescriptor(o, 'foo').get);
  assertEquals(undefined, Object.getOwnPropertyDescriptor(o, 'foo').set());
}

{
  const o = {
    set foo(_) { },
    foo: 666,
    get foo() { return 42 },
  };
  assertEquals(42, Object.getOwnPropertyDescriptor(o, 'foo').get());
}
