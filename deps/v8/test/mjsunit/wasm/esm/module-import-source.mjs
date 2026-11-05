// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --js-source-phase-imports --allow-natives-syntax

import source mod from '../incrementer.wasm';
import source async from '../incrementer.wasm';
import source get from '../incrementer.wasm';
import source set from '../incrementer.wasm';
import source using from '../incrementer.wasm';
import source of from '../incrementer.wasm';
import source accessor from '../incrementer.wasm';

assertEquals(Object.getPrototypeOf(mod), WebAssembly.Module.prototype);
assertEquals(mod[Symbol.toStringTag], 'WebAssembly.Module');
assertEquals(async, mod);
assertEquals(get, mod);
assertEquals(set, mod);
assertEquals(using, mod);
assertEquals(of, mod);
assertEquals(accessor, mod);

var AbstractModuleSource = %GetAbstractModuleSource();
var ToStringTag = Object
  .getOwnPropertyDescriptor(
    AbstractModuleSource.prototype,
    Symbol.toStringTag,
  ).get;
assertTrue(mod instanceof AbstractModuleSource);
assertEquals(ToStringTag.call(mod), "WebAssembly.Module");
