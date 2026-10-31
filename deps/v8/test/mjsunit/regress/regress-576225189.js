// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A groups object returned from the regexp results cache must be created in
// the calling realm, not in the realm that filled the entry.

// The subject is longer than kMinLengthToCache and, as a string literal,
// internalized, so all realms evaluating this source use the same subject.
// The regexp data is shared through the compilation cache.
const kReplace = `(function() {
  let groups;
  "${'xy'.repeat(0x900)}".replace(/(?<n>x)y/g, function(...args) {
    if (groups === undefined) groups = args[args.length - 1];
    return '';
  });
  return groups;
})()`;

// Whether the groups object was created in the evaluating realm.
// Error.captureStackTrace installs an accessor whose getter is instantiated in
// the holder's creation realm.
const kCheck = `(function(groups) {
  if (groups.n !== 'x') return false;
  if (Object.getPrototypeOf(groups) !== null) return false;
  Error.captureStackTrace(groups);
  const getter = Object.getOwnPropertyDescriptor(groups, 'stack').get;
  return Object.getPrototypeOf(getter) === Function.prototype;
})(${kReplace})`;

const other = Realm.create();

// Repeated calls per realm also exercise same-realm cache hits.
assertTrue(Realm.eval(other, kCheck));
for (let i = 0; i < 3; i++) assertTrue(eval(kCheck));
for (let i = 0; i < 3; i++) assertTrue(Realm.eval(other, kCheck));
assertTrue(eval(kCheck));
