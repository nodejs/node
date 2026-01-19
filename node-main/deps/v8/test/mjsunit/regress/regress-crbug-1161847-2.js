// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(first_run) {
  let o = { x: 0 };
  if (first_run) assertTrue(%HasOwnConstDataProperty(o, 'x'));
  Object.defineProperty(o, 'x', { get() { return 1; }, configurable: true, enumerable: true });
  delete o.x;
  o.x = 23;

  if (%IsDictPropertyConstTrackingEnabled()) {
    // TODO(11248, ishell) Adding a property always sets it to constant if
    // V8_DICT_PROPERTY_CONST_TRACKING is enabled, even if the property was
    // deleted before and is re-added. See
    // LookupIterator::PrepareTransitionToDataProperty, specically the usage of
    // PropertyDetails::kConstIfDictConstnessTracking in there.
    return;
  }

  if (first_run) assertFalse(%HasOwnConstDataProperty(o, 'x'));
}
%PrepareFunctionForOptimization(foo);
foo(true);
foo(false);
%OptimizeFunctionOnNextCall(foo);
foo(false);
