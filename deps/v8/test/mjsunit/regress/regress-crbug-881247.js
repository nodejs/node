// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const resolvedPromise = Promise.resolve();

function spread() {
  const result = { ...resolvedPromise };
  %HeapObjectVerify(result);
  return result;
}

resolvedPromise[undefined] =  {a:1};
%HeapObjectVerify(resolvedPromise);

spread();

resolvedPromise[undefined] = undefined;
%HeapObjectVerify(resolvedPromise);

spread();
%HeapObjectVerify(resolvedPromise);
