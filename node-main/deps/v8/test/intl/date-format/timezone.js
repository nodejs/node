// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Tests time zone support.

// TODO(littledan): getDefaultTimeZone() is not available from JavaScript
// var df = Intl.DateTimeFormat();
// assertEquals(getDefaultTimeZone(), df.resolvedOptions().timeZone);

[
  'UtC', 'gmt', 'Etc/UTC', 'Etc/GMT', 'Etc/GMT0', 'Etc/GMT+0',
  'etc/gmt-0', 'etc/zulu', 'Etc/universal', 'etc/greenwich'
].forEach((timezone) => {
  const df = Intl.DateTimeFormat(undefined, {timeZone: timezone});
  assertEquals('UTC', df.resolvedOptions().timeZone);
})

// See test/mjsunit/regress/regress-crbug-364374.js for additional/ tests.

df = Intl.DateTimeFormat(undefined, {timeZone: 'America/Los_Angeles'});
assertEquals('America/Los_Angeles', df.resolvedOptions().timeZone);

df = Intl.DateTimeFormat(undefined, {timeZone: 'Europe/Belgrade'});
assertEquals('Europe/Belgrade', df.resolvedOptions().timeZone);

df = Intl.DateTimeFormat(undefined, {timeZone: 'euRope/beLGRade'});
assertEquals('Europe/Belgrade', df.resolvedOptions().timeZone);

// Etc/GMT-14 to Etc/GMT+12 are valid.
df = Intl.DateTimeFormat(undefined, {timeZone: 'etc/gmt+12'});
assertEquals('Etc/GMT+12', df.resolvedOptions().timeZone);

df = Intl.DateTimeFormat(undefined, {timeZone: 'etc/gmt+9'});
assertEquals('Etc/GMT+9', df.resolvedOptions().timeZone);

df = Intl.DateTimeFormat(undefined, {timeZone: 'etc/gmt-9'});
assertEquals('Etc/GMT-9', df.resolvedOptions().timeZone);

df = Intl.DateTimeFormat(undefined, {timeZone: 'etc/gmt-14'});
assertEquals('Etc/GMT-14', df.resolvedOptions().timeZone);

assertThrows('Intl.DateTimeFormat(undefined, {timeZone: \'Etc/GMT+13\'})');

// : + - are not allowed, only / _ are.
assertThrows('Intl.DateTimeFormat(undefined, {timeZone: \'GMT+07:00\'})');
assertThrows('Intl.DateTimeFormat(undefined, {timeZone: \'GMT+0700\'})');
assertThrows('Intl.DateTimeFormat(undefined, {timeZone: \'GMT-05:00\'})');
assertThrows('Intl.DateTimeFormat(undefined, {timeZone: \'GMT-0500\'})');
assertThrows('Intl.DateTimeFormat(undefined, ' +
    '{timeZone: \'America/Los-Angeles\'})');

// Throws for unsupported time zones.
assertThrows('Intl.DateTimeFormat(undefined, {timeZone: \'Aurope/Belgrade\'})');
