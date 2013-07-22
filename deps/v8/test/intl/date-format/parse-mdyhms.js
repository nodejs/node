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

// Testing v8Parse method for date and time pattern.

var dtf = new Intl.DateTimeFormat(['en'],
                                  {year: 'numeric', month: 'numeric',
                                   day: 'numeric', hour: 'numeric',
                                   minute: 'numeric', second: 'numeric'});

// Make sure we have pattern we expect (may change in the future).
assertEquals('M/d/y h:mm:ss a', dtf.resolved.pattern);

assertEquals('Sat May 04 1974 12:30:12 GMT-0007 (PDT)',
             usePDT(String(dtf.v8Parse('5/4/74 12:30:12 pm'))));

// AM/PM were not specified.
assertEquals(undefined, dtf.v8Parse('5/4/74 12:30:12'));

// Time was not specified.
assertEquals(undefined, dtf.v8Parse('5/4/74'));

// Month is numeric, so it fails on "May".
assertEquals(undefined, dtf.v8Parse('May 4th 1974'));

// Wrong date delimiter.
assertEquals(undefined, dtf.v8Parse('5-4-74 12:30:12 am'));
