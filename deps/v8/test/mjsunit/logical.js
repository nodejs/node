// Copyright 2011 the V8 project authors. All rights reserved.
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

var undef = void 0;

assertEquals(1, 1 || 0);
assertEquals(2, 0 || 2);
assertEquals('foo', 0 || 'foo');
assertEquals(undef, undef || undef);
assertEquals('foo', 'foo' || 'bar');
assertEquals('bar', undef || 'bar');
assertEquals('bar', undef || 'bar' || 'baz');
assertEquals('baz', undef || undef || 'baz');

assertEquals(0, 1 && 0);
assertEquals(0, 0 && 2);
assertEquals(0, 0 && 'foo');
assertEquals(undef, undef && undef);
assertEquals('bar', 'foo' && 'bar');
assertEquals(undef, undef && 'bar');
assertEquals('baz', 'foo' && 'bar' && 'baz');
assertEquals(undef, 'foo' && undef && 'baz');

assertEquals(0, undef && undef || 0);
assertEquals('bar', undef && 0 || 'bar');
