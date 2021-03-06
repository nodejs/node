// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should never be called in this test.
Error.prepareStackTrace = () => 299792458;

{
  const that_realm = Realm.create();
  const result = Realm.eval(that_realm,
      "() => { Error.prepareStackTrace = () => 42; return new Error(); }")();
  assertEquals(42, result.stack);
}
{
  const that_realm = Realm.create();
  const result = Realm.eval(that_realm,
      "() => { Error.prepareStackTrace = () => 42; " +
      "class MyError extends Error {}; return new MyError(); }")();
  assertEquals(42, result.stack);
}
{
  const that_realm = Realm.create();
  const result = Realm.eval(that_realm,
      "() => { Error.prepareStackTrace = () => 42; return {}; }")();
  assertFalse("stack" in result);
}
