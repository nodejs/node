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

// Check that reduce and reduceRight call the callback function with
// undefined as the receiver (which for non-strict functions is
// transformed to the global object).

// Check receiver for reduce and reduceRight.

var global = this;
function non_strict(){ assertEquals(global, this); }
function strict(){ "use strict"; assertEquals(void 0, this); }
function strict_null(){ "use strict"; assertEquals(null, this); }

[2, 3].reduce(non_strict);
[2, 3].reduce(strict);
[2, 3].reduceRight(non_strict);
[2, 3].reduceRight(strict);


// Check the receiver for callbacks in other array methods.
[2, 3].every(non_strict);
[2, 3].every(non_strict, undefined);
[2, 3].every(non_strict, null);
[2, 3].every(strict);
[2, 3].every(strict, undefined);
[2, 3].every(strict_null, null);

[2, 3].filter(non_strict);
[2, 3].filter(non_strict, undefined);
[2, 3].filter(non_strict, null);
[2, 3].filter(strict);
[2, 3].filter(strict, undefined);
[2, 3].filter(strict_null, null);

[2, 3].forEach(non_strict);
[2, 3].forEach(non_strict, undefined);
[2, 3].forEach(non_strict, null);
[2, 3].forEach(strict);
[2, 3].forEach(strict, undefined);
[2, 3].forEach(strict_null, null);

[2, 3].map(non_strict);
[2, 3].map(non_strict, undefined);
[2, 3].map(non_strict, null);
[2, 3].map(strict);
[2, 3].map(strict, undefined);
[2, 3].map(strict_null, null);

[2, 3].some(non_strict);
[2, 3].some(non_strict, undefined);
[2, 3].some(non_strict, null);
[2, 3].some(strict);
[2, 3].some(strict, undefined);
[2, 3].some(strict_null, null);
