// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The regexp results cache kicks in for internalized subjects above a length
// threshold. Its entries must not hand the same groups object to more than one
// replace callback.

// Longer than kMinLengthToCache, and internalized: string literals are
// internalized when their script is compiled, hence the eval.
const kLong = eval('"' + 'xy'.repeat(0x900) + '"');
const kShort = eval('"' + 'xy'.repeat(4) + '"');
const kNotInternalized = ('xy'.repeat(0x900) + 'z').slice(0, -1);

// Returns the last argument the first invocation of the callback received,
// which is the groups object for patterns that have named captures.
function firstLastArg(subject, re, mutate = false) {
  let seen;
  subject.replace(re, function(...args) {
    if (seen === undefined) {
      seen = args[args.length - 1];
      if (mutate) {
        seen.n = 'mutated';
        seen.added = 'added';
      }
    }
    return '';
  });
  return seen;
}

const kNamed = /(?<n>x)y/g;

// Populates the cache, then mutates the groups object it handed out.
const poisoned = firstLastArg(kLong, kNamed, true);

const groups = firstLastArg(kLong, kNamed);
assertNotSame(poisoned, groups);
assertEquals('x', groups.n);
assertFalse(Object.hasOwn(groups, 'added'));
assertEquals(null, Object.getPrototypeOf(groups));

// The copying is gated on the pattern having named captures, and only runs
// where the cache applies at all. Each condition below skips it; the last
// argument is then the subject string rather than a groups object.
assertSame(kLong, firstLastArg(kLong, /xy/g));
assertSame(kLong, firstLastArg(kLong, /(x)y/g));

// Below the length threshold, and non-internalized: the cache is never
// consulted, so each call builds its own groups object.
for (const subject of [kShort, kNotInternalized]) {
  const first = firstLastArg(subject, kNamed, true);
  const second = firstLastArg(subject, kNamed);
  assertNotSame(first, second);
  assertEquals('x', second.n);
}
