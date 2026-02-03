// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function TestRegisterWithSymbolTarget() {
  const fg = new FinalizationRegistry(() => { });
  fg.register(Symbol('123'), 'holdings');
  // Registered symbols cannot be the target.
  assertThrows(() => fg.register(Symbol.for('123'), 'holdings'), TypeError);
})();

(function TestRegisterWithSymbolUnregisterToken() {
  const fg = new FinalizationRegistry(() => { });
  fg.register({}, 'holdings', Symbol('123'));
  // Registered symbols cannot be the unregister token.
  assertThrows(() => fg.register({}, 'holdings', Symbol.for('123')), TypeError);
})();

(function TestRegisterSymbolAndHoldingsSameValue() {
  const fg = new FinalizationRegistry(() => {});
  const obj = Symbol('123');
  // SameValue(target, holdings) not ok.
  assertThrows(() => fg.register(obj, obj), TypeError);
  const holdings = {a: 1};
  fg.register(obj, holdings);
})();

(function TestUnregisterWithSymbolUnregisterToken() {
  const fg = new FinalizationRegistry(() => {});
  fg.unregister(Symbol('123'));
  // Registered symbols cannot be the unregister token.
  assertThrows(() => fg.unregister(Symbol.for('123')), TypeError);
})();

(function TestWeakRefConstructorWithSymbol() {
  new WeakRef(Symbol('123'));
  // Registered symbols cannot be the WeakRef target.
  assertThrows(() => new WeakRef(Symbol.for('123')), TypeError);
})();
