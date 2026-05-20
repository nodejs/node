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

// Sort plain German text using defaults.

var strings = ['März', 'Fuße', 'FUSSE', 'Fluße', 'Flusse',
               'flusse', 'fluße', 'flüße', 'flüsse'];

var collator = Intl.Collator(['de']);
var result = strings.sort(collator.compare);

assertEquals('flusse', result[0]);
assertEquals('Flusse', result[1]);
assertEquals('fluße', result[2]);
assertEquals('Fluße', result[3]);
assertEquals('flüsse', result[4]);
assertEquals('flüße', result[5]);
assertEquals('FUSSE', result[6]);
assertEquals('Fuße', result[7]);
assertEquals('März', result[8]);

result = ["AE", "Ä"].sort(new Intl.Collator("de", {usage: "sort"}).compare)
assertEquals("Ä", result[0]);
assertEquals("AE", result[1]);
result = ["AE", "Ä"].sort(new Intl.Collator("de", {usage: "search"}).compare)
assertEquals("AE", result[0]);
assertEquals("Ä", result[1]);


var collator = new Intl.Collator("de", {usage: "search"});
collator.resolvedOptions() // This triggers the code that removes the u-co-search keyword
result = ["AE", "Ä"].sort(collator.compare)
assertEquals("AE", result[0]);
assertEquals("Ä", result[1]);
