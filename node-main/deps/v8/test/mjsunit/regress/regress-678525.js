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

assertEquals(0,  '\0'.charCodeAt(0));
assertEquals(1,  '\1'.charCodeAt(0));
assertEquals(2,  '\2'.charCodeAt(0));
assertEquals(3,  '\3'.charCodeAt(0));
assertEquals(4,  '\4'.charCodeAt(0));
assertEquals(5,  '\5'.charCodeAt(0));
assertEquals(6,  '\6'.charCodeAt(0));
assertEquals(7,  '\7'.charCodeAt(0));
assertEquals(56, '\8'.charCodeAt(0));

assertEquals('\010', '\10');
assertEquals('\011', '\11');
assertEquals('\012', '\12');
assertEquals('\013', '\13');
assertEquals('\014', '\14');
assertEquals('\015', '\15');
assertEquals('\016', '\16');
assertEquals('\017', '\17');

assertEquals('\020', '\20');
assertEquals('\021', '\21');
assertEquals('\022', '\22');
assertEquals('\023', '\23');
assertEquals('\024', '\24');
assertEquals('\025', '\25');
assertEquals('\026', '\26');
assertEquals('\027', '\27');

assertEquals(73,  '\111'.charCodeAt(0));
assertEquals(105, '\151'.charCodeAt(0));
