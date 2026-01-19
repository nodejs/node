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

// Test if resolvedOptions() returns expected fields/values.

// Default (year, month, day) formatter.
var dtfDefault = Intl.DateTimeFormat('en-US');
var resolved = dtfDefault.resolvedOptions();

assertTrue(resolved.hasOwnProperty('locale'));
assertEquals('en-US', resolved.locale);
assertTrue(resolved.hasOwnProperty('numberingSystem'));
assertEquals('latn', resolved.numberingSystem);
assertTrue(resolved.hasOwnProperty('calendar'));
assertEquals('gregory', resolved.calendar);
assertTrue(resolved.hasOwnProperty('timeZone'));
// TODO(littledan): getDefaultTimeZone() is not available from JavaScript
// assertEquals(getDefaultTimeZone(), resolved.timeZone);
// These are in by default.
assertTrue(resolved.hasOwnProperty('year'));
assertEquals('numeric', resolved.year);
assertTrue(resolved.hasOwnProperty('month'));
assertEquals('numeric', resolved.month);
assertTrue(resolved.hasOwnProperty('day'));
assertEquals('numeric', resolved.day);
// These shouldn't be in by default.
assertFalse(resolved.hasOwnProperty('era'));
assertFalse(resolved.hasOwnProperty('timeZoneName'));
assertFalse(resolved.hasOwnProperty('weekday'));
assertFalse(resolved.hasOwnProperty('hour12'));
assertFalse(resolved.hasOwnProperty('hour'));
assertFalse(resolved.hasOwnProperty('minute'));
assertFalse(resolved.hasOwnProperty('second'));

// Time formatter.
var dtfTime = Intl.DateTimeFormat(
  'sr-RS', {hour: 'numeric', minute: 'numeric', second: 'numeric'});
resolved = dtfTime.resolvedOptions();

assertTrue(resolved.hasOwnProperty('locale'));
assertTrue(resolved.hasOwnProperty('numberingSystem'));
assertTrue(resolved.hasOwnProperty('calendar'));
assertTrue(resolved.hasOwnProperty('timeZone'));
assertTrue(resolved.hasOwnProperty('hour12'));
assertEquals(false, resolved.hour12);
assertTrue(resolved.hasOwnProperty('hour'));
assertEquals('2-digit', resolved.hour);
assertTrue(resolved.hasOwnProperty('minute'));
assertEquals('2-digit', resolved.minute);
assertTrue(resolved.hasOwnProperty('second'));
assertEquals('2-digit', resolved.second);
// Didn't ask for them.
assertFalse(resolved.hasOwnProperty('year'));
assertFalse(resolved.hasOwnProperty('month'));
assertFalse(resolved.hasOwnProperty('day'));
assertFalse(resolved.hasOwnProperty('era'));
assertFalse(resolved.hasOwnProperty('timeZoneName'));
assertFalse(resolved.hasOwnProperty('weekday'));

// Full formatter.
var dtfFull = Intl.DateTimeFormat(
  'en-US', {weekday: 'short', era: 'short', year: 'numeric', month: 'short',
            day: 'numeric', hour: 'numeric', minute: 'numeric',
            second: 'numeric', timeZoneName: 'short', timeZone: 'UTC'});
resolved = dtfFull.resolvedOptions();

assertTrue(resolved.hasOwnProperty('locale'));
assertTrue(resolved.hasOwnProperty('numberingSystem'));
assertTrue(resolved.hasOwnProperty('calendar'));
assertTrue(resolved.hasOwnProperty('timeZone'));
assertTrue(resolved.hasOwnProperty('hour12'));
assertEquals(true, resolved.hour12);
assertTrue(resolved.hasOwnProperty('hour'));
assertTrue(resolved.hasOwnProperty('minute'));
assertTrue(resolved.hasOwnProperty('second'));
assertTrue(resolved.hasOwnProperty('year'));
assertTrue(resolved.hasOwnProperty('month'));
assertTrue(resolved.hasOwnProperty('day'));
assertTrue(resolved.hasOwnProperty('era'));
assertEquals('short', resolved.era);
assertTrue(resolved.hasOwnProperty('timeZoneName'));
assertEquals('short', resolved.timeZoneName);
assertTrue(resolved.hasOwnProperty('weekday'));
assertEquals('short', resolved.weekday);
