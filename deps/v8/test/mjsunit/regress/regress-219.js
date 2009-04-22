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

re = /a/ii;
assertFlags(re, false, false, true)

re = /a/gii;
assertFlags(re, true, false, true)

re = /a/igi;
assertFlags(re, true, false, true)

re = /a/iig;
assertFlags(re, true, false, true)

re = /a/gimi;
assertFlags(re, true, true, true)

re = /a/giim;
assertFlags(re, true, true, true)

re = /a/igim;
assertFlags(re, true, true, true)


re = RegExp("a", "ii");
assertFlags(re, false, false, true)

re = RegExp("a", "gii");
assertFlags(re, true, false, true)

re = RegExp("a", "igi");
assertFlags(re, true, false, true)

re = RegExp("a", "iig");
assertFlags(re, true, false, true)

re = RegExp("a", "gimi");
assertFlags(re, true, true, true)

re = RegExp("a", "giim");
assertFlags(re, true, true, true)

re = RegExp("a", "igim");
assertFlags(re, true, true, true)

// Tripple i's

re = /a/iii;
assertFlags(re, false, false, true)

re = /a/giii;
assertFlags(re, true, false, true)

re = /a/igii;
assertFlags(re, true, false, true)

re = /a/iigi;
assertFlags(re, true, false, true)

re = /a/iiig;
assertFlags(re, true, false, true)

re = /a/miiig;
assertFlags(re, true, true, true)


re = RegExp("a", "iii");
assertFlags(re, false, false, true)

re = RegExp("a", "giii");
assertFlags(re, true, false, true)

re = RegExp("a", "igii");
assertFlags(re, true, false, true)

re = RegExp("a", "iigi");
assertFlags(re, true, false, true)

re = RegExp("a", "iiig");
assertFlags(re, true, false, true)

re = RegExp("a", "miiig");
assertFlags(re, true, true, true)

// Illegal flags - flags late in string.

re = /a/arglebargleglopglyf;
assertFlags(re, true, false, false)

re = /a/arglebargleglopglif;
assertFlags(re, true, false, true)

re = /a/arglebargleglopglym;
assertFlags(re, true, true, false)

re = /a/arglebargleglopglim;
assertFlags(re, true, true, true)

// Case of flags still matters.

re = /a/gmi;
assertFlags(re, true, true, true)

re = /a/Gmi;
assertFlags(re, false, true, true)

re = /a/gMi;
assertFlags(re, true, false, true)

re = /a/gmI;
assertFlags(re, true, true, false)

re = /a/GMi;
assertFlags(re, false, false, true)

re = /a/GmI;
assertFlags(re, false, true, false)

re = /a/gMI;
assertFlags(re, true, false, false)

re = /a/GMI;
assertFlags(re, false, false, false)
