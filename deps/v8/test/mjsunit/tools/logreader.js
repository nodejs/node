// Copyright 2009 the V8 project authors. All rights reserved.
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

// Load CSV Parser and Log Reader implementations from <project root>/tools.
// Files: tools/csvparser.js tools/logreader.js


(function testAddressParser() {
  var reader = new devtools.profiler.LogReader({});
  var parser = reader.createAddressParser('test');

  // Test that 0x values are parsed, and prevAddresses_ are untouched.
  assertFalse('test' in reader.prevAddresses_);
  assertEquals(0, parser('0x0'));
  assertFalse('test' in reader.prevAddresses_);
  assertEquals(0x100, parser('0x100'));
  assertFalse('test' in reader.prevAddresses_);
  assertEquals(0xffffffff, parser('0xffffffff'));
  assertFalse('test' in reader.prevAddresses_);

  // Test that values that has no '+' or '-' prefix are parsed
  // and saved to prevAddresses_.
  assertEquals(0, parser('0'));
  assertEquals(0, reader.prevAddresses_.test);
  assertEquals(0x100, parser('100'));
  assertEquals(0x100, reader.prevAddresses_.test);
  assertEquals(0xffffffff, parser('ffffffff'));
  assertEquals(0xffffffff, reader.prevAddresses_.test);

  // Test that values prefixed with '+' or '-' are treated as deltas,
  // and prevAddresses_ is updated.
  // Set base value.
  assertEquals(0x100, parser('100'));
  assertEquals(0x100, reader.prevAddresses_.test);
  assertEquals(0x200, parser('+100'));
  assertEquals(0x200, reader.prevAddresses_.test);
  assertEquals(0x100, parser('-100'));
  assertEquals(0x100, reader.prevAddresses_.test);
})();


(function testAddressParser() {
  var reader = new devtools.profiler.LogReader({});

  assertEquals([0x10000000, 0x10001000, 0xffff000, 0x10000000],
               reader.processStack(0x10000000, ['overflow',
                   '+1000', '-2000', '+1000']));
})();


(function testExpandBackRef() {
  var reader = new devtools.profiler.LogReader({});

  assertEquals('aaaaaaaa', reader.expandBackRef_('aaaaaaaa'));
  assertEquals('aaaaaaaa', reader.expandBackRef_('#1'));
  assertEquals('bbbbaaaa', reader.expandBackRef_('bbbb#2:4'));
  assertEquals('"#1:1"', reader.expandBackRef_('"#1:1"'));
})();
