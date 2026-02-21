// Copyright 2008 the V8 project authors. All rights reserved.
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

// Tests handling of flags for regexps.

// We should now allow duplicates of flags.
// (See http://code.google.com/p/v8/issues/detail?id=219)

// This has been reversed by issue 1628, since other browsers have also
// tightened their syntax.
// (See http://code.google.com/p/v8/issues/detail?id=1628)

// Base tests: we recognize the basic flags

function assertFlags(re, global, multiline, ignoreCase) {
  var name = re + " flag: ";
  (global ? assertTrue : assertFalse)(re.global, name + "g");
  (multiline ? assertTrue : assertFalse)(re.multiline, name + "m");
  (ignoreCase ? assertTrue : assertFalse)(re.ignoreCase, name + "i");
}

var re = /a/;
assertFlags(re, false, false, false)

re = /a/gim;
assertFlags(re, true, true, true)

re = RegExp("a","");
assertFlags(re, false, false, false)

re = RegExp("a", "gim");
assertFlags(re, true, true, true)

// Double i's

assertThrows("/a/ii");

assertThrows("/a/gii");

assertThrows("/a/igi");

assertThrows("/a/iig");

assertThrows("/a/gimi");

assertThrows("/a/giim");

assertThrows("/a/igim");

assertThrows(function(){ return RegExp("a", "ii"); })

assertThrows(function(){ return RegExp("a", "gii"); })

assertThrows(function(){ return RegExp("a", "igi"); })

assertThrows(function(){ return RegExp("a", "iig"); })

assertThrows(function(){ return RegExp("a", "gimi"); })

assertThrows(function(){ return RegExp("a", "giim"); })

assertThrows(function(){ return RegExp("a", "igim"); })

// Tripple i's

assertThrows("/a/iii");

assertThrows("/a/giii");

assertThrows("/a/igii");

assertThrows("/a/iigi");

assertThrows("/a/iiig");

assertThrows("/a/miiig");

assertThrows(function(){ return RegExp("a", "iii"); })

assertThrows(function(){ return RegExp("a", "giii"); })

assertThrows(function(){ return RegExp("a", "igii"); })

assertThrows(function(){ return RegExp("a", "iigi"); })

assertThrows(function(){ return RegExp("a", "iiig"); })

assertThrows(function(){ return RegExp("a", "miiig"); })

// Illegal flags - valid flags late in string.

assertThrows("/a/arglebargleglopglyf");

assertThrows("/a/arglebargleglopglif");

assertThrows("/a/arglebargleglopglym");

assertThrows("/a/arglebargleglopglim");

// Case of flags still matters.

var re = /a/gmi;
assertFlags(re, true, true, true)

assertThrows("/a/Gmi");

assertThrows("/a/gMi");

assertThrows("/a/gmI");

assertThrows("/a/GMi");

assertThrows("/a/GmI");

assertThrows("/a/gMI");

assertThrows("/a/GMI");

// Unicode escape sequences are not interpreted.

assertThrows("/a/\\u0067");
assertThrows("/a/\\u0069");
assertThrows("/a/\\u006d");
assertThrows("/a/\\u006D");
