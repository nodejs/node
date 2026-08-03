// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var log = [];

function check(predicate, item) {
  if (!predicate) log.push(item);
}

var global = this;

Object.getOwnPropertyNames(global).forEach(function(name) {
  // Only check for global properties with uppercase names.
  if (name[0] != name[0].toUpperCase()) return;

  var obj = global[name];

  // Skip non-receivers.
  if (!%IsJSReceiver(obj)) return;

  // Skip non-natives.
  if (!obj.toString().includes('native')) return;

  // Construct an instance.
  try {
    new obj();
  } catch (e) {
  }

  // Check the object.
  check(%HasFastProperties(obj), `${name}`);

  // Check the constructor.
  var constructor = obj.constructor;
  if (!%IsJSReceiver(constructor)) return;
  check(%HasFastProperties(constructor), `${name}.constructor`);

  // Check the prototype.
  var prototype = obj.prototype;
  if (!%IsJSReceiver(prototype)) return;
  check(%HasFastProperties(prototype), `${name}.prototype`);

  // Check the prototype.constructor.
  var prototype_constructor = prototype.constructor;
  if (!%IsJSReceiver(prototype_constructor)) return;
  check(
      %HasFastProperties(prototype_constructor),
      `${name}.prototype.constructor`);
});

// There should be no dictionary mode builtin objects.
if (!%IsDictPropertyConstTrackingEnabled())
  assertEquals([], log);
