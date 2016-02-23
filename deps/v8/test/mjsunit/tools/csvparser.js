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

// Load CSV parser implementation from <project root>/tools.
// Files: tools/csvparser.js

var parser = new CsvParser();

assertEquals(
    [],
    parser.parseLine(''));

assertEquals(
    ['', ''],
    parser.parseLine(','));

assertEquals(
    ['1997','Ford','E350'],
    parser.parseLine('1997,Ford,E350'));

assertEquals(
    ['1997','Ford','E350'],
    parser.parseLine('"1997","Ford","E350"'));

assertEquals(
    ['1997','Ford','E350','Super, luxurious truck'],
    parser.parseLine('1997,Ford,E350,"Super, luxurious truck"'));

assertEquals(
    ['1997','Ford','E350','Super "luxurious" truck'],
    parser.parseLine('1997,Ford,E350,"Super ""luxurious"" truck"'));

assertEquals(
    ['1997','Ford','E350','Super "luxurious" "truck"'],
    parser.parseLine('1997,Ford,E350,"Super ""luxurious"" ""truck"""'));

assertEquals(
    ['1997','Ford','E350','Super "luxurious""truck"'],
    parser.parseLine('1997,Ford,E350,"Super ""luxurious""""truck"""'));

assertEquals(
    ['shared-library','/lib/ld-2.3.6.so','0x489a2000','0x489b7000'],
    parser.parseLine('shared-library,"/lib/ld-2.3.6.so",0x489a2000,0x489b7000'));

assertEquals(
    ['code-creation','LazyCompile','0xf6fe2d20','1201','APPLY_PREPARE native runtime.js:165'],
    parser.parseLine('code-creation,LazyCompile,0xf6fe2d20,1201,"APPLY_PREPARE native runtime.js:165"'));

assertEquals(
    ['code-creation','LazyCompile','0xf6fe4bc0','282',' native v8natives.js:69'],
    parser.parseLine('code-creation,LazyCompile,0xf6fe4bc0,282," native v8natives.js:69"'));

assertEquals(
    ['code-creation','RegExp','0xf6c21c00','826','NccyrJroXvg\\/([^,]*)'],
    parser.parseLine('code-creation,RegExp,0xf6c21c00,826,"NccyrJroXvg\\/([^,]*)"'));

assertEquals(
    ['code-creation','Function','0x42f0a0','163',''],
    parser.parseLine('code-creation,Function,0x42f0a0,163,""'));
