// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function makeLoggingNewTarget(Target, log) {
  return new Proxy(Target, {
    get(target, prop, receiver) {
      if (prop === 'prototype') {
        log.push('prototype');
      }
      return Reflect.get(target, prop, receiver);
    }
  });
}

(function TestGlobalValueConversionOrder() {
  const log = [];
  const newTarget = makeLoggingNewTarget(WebAssembly.Global, log);
  const global = Reflect.construct(
      WebAssembly.Global,
      [
        {value: 'i32'},
        {
          valueOf() {
            log.push('valueOf');
            return 42;
          }
        }
      ],
      newTarget);
  assertEquals(42, global.value);
  assertEquals(['valueOf', 'prototype'], log);
})();

(function TestGlobalTypeValidationBeforePrototypeLookup() {
  const log = [];
  const newTarget = makeLoggingNewTarget(WebAssembly.Global, log);
  assertThrows(
      () => Reflect.construct(WebAssembly.Global, [{value: 'v128'}], newTarget),
      TypeError);
  assertEquals([], log);
})();

(function TestExceptionArgumentsConversionOrder() {
  const log = [];
  const tag = new WebAssembly.Tag({parameters: ['i32']});
  const newTarget = makeLoggingNewTarget(WebAssembly.Exception, log);
  const ex = Reflect.construct(
      WebAssembly.Exception,
      [
        tag,
        [
          {
            valueOf() {
              log.push('payload');
              return 7;
            }
          }
        ],
        {
          get traceStack() {
            log.push('traceStack');
            return false;
          }
        }
      ],
      newTarget);
  assertEquals(7, ex.getArg(tag, 0));
  assertEquals(['payload', 'traceStack', 'prototype'], log);
})();
