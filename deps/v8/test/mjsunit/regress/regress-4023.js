// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --block-concurrent-recompilation

function Inner() {
  this.property = "OK";
  this.prop2 = 1;
}

function Outer() {
  this.o = "u";
}
function KeepMapAlive(o) {
  return o.o;
}
function SetInner(o, i) {
  o.inner_field = i;
}
function Crash(o) {
  return o.inner_field.property;
}

var inner = new Inner();
var outer = new Outer();

// Collect type feedback.
SetInner(new Outer(), inner);
SetInner(outer, inner);

// This function's only purpose is to stash away a Handle that keeps
// outer's map alive during the gc() call below. We store this handle
// on the compiler thread :-)
KeepMapAlive(outer);
KeepMapAlive(outer);
%OptimizeFunctionOnNextCall(KeepMapAlive, "concurrent");
KeepMapAlive(outer);

// So far, all is well. Collect type feedback and optimize.
print(Crash(outer));
print(Crash(outer));
%OptimizeFunctionOnNextCall(Crash);
print(Crash(outer));

// Null out references and perform GC. This will keep outer's map alive
// (due to the handle created above), but will let inner's map die. Hence,
// inner_field's field type stored in outer's map will get cleared.
inner = undefined;
outer = undefined;
gc();

// We could unblock the compiler thread now. But why bother?

// Now optimize SetInner while inner_field's type is still cleared!
// This will generate optimized code that stores arbitrary objects
// into inner_field without checking their type against the field type.
%OptimizeFunctionOnNextCall(SetInner);

// Use the optimized code to store an arbitrary object into
// o2's inner_field, without triggering any dependent code deopts...
var o2 = new Outer();
SetInner(o2, { invalid: 1.51, property: "OK" });
// ...and then use the existing code expecting an Inner-class object to
// read invalid data (in this case, a raw double).
// We crash trying to convert the raw double into a printable string.
print(Crash(o2));
