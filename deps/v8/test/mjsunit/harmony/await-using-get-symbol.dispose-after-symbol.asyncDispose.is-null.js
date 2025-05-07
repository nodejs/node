// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-staging

let order = [];

async function TestSymbolAsyncDisposeAndSymbolDisposeGetOrder() {
  var resource = {
      get [Symbol.asyncDispose]() {
        order.push('Symbol.asyncDispose');
        return null;
      },
      get [Symbol.dispose]() {
          order.push('Symbol.dispose');
          return function() { };
      }
  };

  await using x = resource;
}

async function RunTest() {
  await TestSymbolAsyncDisposeAndSymbolDisposeGetOrder();
  assertSame('Symbol.asyncDispose,Symbol.dispose', order.join(','));
}

RunTest();
