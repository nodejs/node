// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-defer-import-eval

const wasm_url = 'data:text/javascript,//.wasm';

assertPromiseResult(
    import(wasm_url), assertUnreachable,
    e => assertInstanceof(e, Error));

assertPromiseResult(
    import.defer(wasm_url), assertUnreachable,
    e => assertInstanceof(e, Error));

assertPromiseResult(
    import(`data:text/javascript,import "${wasm_url}";`),
    assertUnreachable, e => assertInstanceof(e, Error));

assertPromiseResult(
    import(`data:text/javascript,import defer * as m from "${wasm_url}";`),
    assertUnreachable, e => assertInstanceof(e, Error));
