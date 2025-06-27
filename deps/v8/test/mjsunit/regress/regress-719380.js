// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

TypeError.prototype.__defineGetter__("name", () => { throw 42; });
try { console.log({ toString: () => { throw new TypeError() }}); } catch (e) {}
try { new WebAssembly.Table({}); } catch (e) {}
