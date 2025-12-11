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

// German eszett
assertEquals("FRIEDRICHSTRASSE 14", "friedrichstra\xDFe 14".toUpperCase());
assertEquals("XXSSSSSSXX", "xx\xDF\xDF\xDFxx".toUpperCase());
assertEquals("(SS)", "(\xDF)".toUpperCase());
assertEquals("SS", "\xDF".toUpperCase());

// Turkish dotted upper-case I lower-case converts to two characters
assertEquals("i\u0307", "\u0130".toLowerCase());
assertEquals("(i\u0307)", "(\u0130)".toLowerCase());
assertEquals("xxi\u0307xx", "XX\u0130XX".toLowerCase());

// Greek small upsilon with dialytika and tonos upper-case converts to three
// characters
assertEquals("\u03A5\u0308\u0301", "\u03B0".toUpperCase());
assertEquals("(\u03A5\u0308\u0301)", "(\u03B0)".toUpperCase());
assertEquals("XX\u03A5\u0308\u0301XX", "xx\u03B0xx".toUpperCase());
