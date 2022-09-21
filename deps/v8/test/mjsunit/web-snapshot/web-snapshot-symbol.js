// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-d8-web-snapshot-api --allow-natives-syntax --verify-heap

'use strict';

d8.file.execute('test/mjsunit/web-snapshot/web-snapshot-helpers.js');

(function TestNonGlobalSymbol() {
  function createObjects() {
    const s = Symbol('description');
    globalThis.foo = {mySymbol: s, innerObject: { symbolHereToo: s}};
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertSame(foo.mySymbol, foo.innerObject.symbolHereToo);
  assertEquals('description', foo.mySymbol.description);
  assertNotEquals(foo.mySymbol, Symbol('description'));
  assertNotEquals(foo.mySymbol, Symbol.for('description'));
})();

(function TestGlobalSymbol() {
  function createObjects() {
    const s = Symbol.for('this is global');
    globalThis.foo = {mySymbol: s, innerObject: { symbolHereToo: s}};
  }
  const {foo} = takeAndUseWebSnapshot(createObjects, ['foo']);
  assertSame(foo.mySymbol, foo.innerObject.symbolHereToo);
  assertEquals('this is global', foo.mySymbol.description);
  assertEquals(Symbol.for('this is global'), foo.mySymbol);
})();

(function TestSymbolAsMapKey() {
  function createObjects() {
    globalThis.obj1 = {};
    const global_symbol = Symbol.for('this is global');
    obj1[global_symbol] = 'global symbol value';
    globalThis.obj2 = {};
    const nonglobal_symbol = Symbol('this is not global');
    obj2[nonglobal_symbol] = 'nonglobal symbol value';
  }
  const {obj1, obj2} = takeAndUseWebSnapshot(createObjects, ['obj1', 'obj2']);
  assertEquals('global symbol value', obj1[Symbol.for('this is global')]);
  const nonglobal_symbol = Object.getOwnPropertySymbols(obj2)[0];
  assertEquals('nonglobal symbol value', obj2[nonglobal_symbol]);
})();
