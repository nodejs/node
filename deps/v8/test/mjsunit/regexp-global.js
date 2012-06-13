// Copyright 2012 the V8 project authors. All rights reserved.
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


// Test that an optional capture is cleared between two matches.
var str = "ABX X";
str = str.replace(/(\w)?X/g, function(match, capture) {
                               assertTrue(match.indexOf(capture) >= 0 ||
                                           capture === undefined);
                               return capture ? capture.toLowerCase() : "-";
                             });
assertEquals("Ab -", str);

// Test zero-length matches.
str = "Als Gregor Samsa eines Morgens";
str = str.replace(/\b/g, function(match, capture) {
                           return "/";
                         });
assertEquals("/Als/ /Gregor/ /Samsa/ /eines/ /Morgens/", str);

// Test zero-length matches that have non-zero-length sub-captures.
str = "It was a pleasure to burn.";
str = str.replace(/(?=(\w+))\b/g, function(match, capture) {
                                    return capture.length;
                                  });
assertEquals("2It 3was 1a 8pleasure 2to 4burn.", str);

// Test multiple captures.
str = "Try not. Do, or do not. There is no try.";
str = str.replace(/(not?)|(do)|(try)/gi,
                  function(match, c1, c2, c3) {
                    assertTrue((c1 === undefined && c2 === undefined) ||
                               (c2 === undefined && c3 === undefined) ||
                               (c1 === undefined && c3 === undefined));
                    if (c1) return "-";
                    if (c2) return "+";
                    if (c3) return "="
                  });
assertEquals("= -. +, or + -. There is - =.", str);

// Test multiple alternate captures.
str = "FOUR LEGS GOOD, TWO LEGS BAD!";
str = str.replace(/(FOUR|TWO) LEGS (GOOD|BAD)/g,
                  function(match, num_legs, likeability) {
                    assertTrue(num_legs !== undefined);
                    assertTrue(likeability !== undefined);
                    if (num_legs == "FOUR") assertTrue(likeability == "GOOD");
                    if (num_legs == "TWO") assertTrue(likeability == "BAD");
                    return match.length - 10;
                  });
assertEquals("4, 2!", str);


// The same tests with UC16.

//Test that an optional capture is cleared between two matches.
str = "AB\u1234 \u1234";
str = str.replace(/(\w)?\u1234/g,
                  function(match, capture) {
                    assertTrue(match.indexOf(capture) >= 0 ||
                               capture === undefined);
                    return capture ? capture.toLowerCase() : "-";
                  });
assertEquals("Ab -", str);

// Test zero-length matches.
str = "Als \u2623\u2642 eines Morgens";
str = str.replace(/\b/g, function(match, capture) {
                           return "/";
                         });
assertEquals("/Als/ \u2623\u2642 /eines/ /Morgens/", str);

// Test zero-length matches that have non-zero-length sub-captures.
str = "It was a pleasure to \u70e7.";
str = str.replace(/(?=(\w+))\b/g, function(match, capture) {
                                    return capture.length;
                                  });
assertEquals("2It 3was 1a 8pleasure 2to \u70e7.", str);

// Test multiple captures.
str = "Try not. D\u26aa, or d\u26aa not. There is no try.";
str = str.replace(/(not?)|(d\u26aa)|(try)/gi,
                  function(match, c1, c2, c3) {
                    assertTrue((c1 === undefined && c2 === undefined) ||
                               (c2 === undefined && c3 === undefined) ||
                               (c1 === undefined && c3 === undefined));
                    if (c1) return "-";
                    if (c2) return "+";
                    if (c3) return "="
                  });
assertEquals("= -. +, or + -. There is - =.", str);

// Test multiple alternate captures.
str = "FOUR \u817f GOOD, TWO \u817f BAD!";
str = str.replace(/(FOUR|TWO) \u817f (GOOD|BAD)/g,
                  function(match, num_legs, likeability) {
                    assertTrue(num_legs !== undefined);
                    assertTrue(likeability !== undefined);
                    if (num_legs == "FOUR") assertTrue(likeability == "GOOD");
                    if (num_legs == "TWO") assertTrue(likeability == "BAD");
                    return match.length - 7;
                  });
assertEquals("4, 2!", str);

// Test capture that is a real substring.
var str = "Beasts of England, beasts of Ireland";
str = str.replace(/(.*)/g, function(match) { return '~'; });
assertEquals("~~", str);

// Test zero-length matches that have non-zero-length sub-captures that do not
// start at the match start position.
str = "up up up up";
str = str.replace(/\b(?=u(p))/g, function(match, capture) {
                                    return capture.length;
                                  });

assertEquals("1up 1up 1up 1up", str);
