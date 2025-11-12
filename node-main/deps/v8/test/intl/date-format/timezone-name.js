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

// Tests time zone names.

// Winter date (PST).
var winter = new Date(2013, 1, 12, 14, 42, 53, 0);

// Summer date (PDT).
var summer = new Date(2013, 7, 12, 14, 42, 53, 0);

// Common flags for both formatters.
var flags = {
  year: 'numeric', month: 'long', day: 'numeric',
  hour : '2-digit', minute : '2-digit', second : '2-digit',
  timeZone: 'America/Los_Angeles'
};

flags.timeZoneName = "short";
var dfs = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dfs.format(winter).indexOf('PST') !== -1);
assertTrue(dfs.format(summer).indexOf('PDT') !== -1);

flags.timeZoneName = "long";
var dfl = new Intl.DateTimeFormat('en-US', flags);

assertTrue(dfl.format(winter).indexOf('Pacific Standard Time') !== -1);
assertTrue(dfl.format(summer).indexOf('Pacific Daylight Time') !== -1);
