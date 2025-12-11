// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests time zone support with conversion.

df = Intl.DateTimeFormat(undefined, {timeZone: 'America/Los_Angeles'});
assertEquals('America/Los_Angeles', df.resolvedOptions().timeZone);

df = Intl.DateTimeFormat(undefined, {timeZone: {toString() { return 'America/Los_Angeles'}}});
assertEquals('America/Los_Angeles', df.resolvedOptions().timeZone);

assertThrows(() => Intl.DateTimeFormat(
    undefined, {timeZone: {toString() { throw new Error("should throw"); }}}));

assertThrows(() => Intl.DateTimeFormat(
    undefined, {get timeZone() { throw new Error("should throw"); }}));
