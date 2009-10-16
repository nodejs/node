// Copyright (c) 2009 Apple Computer, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// 3. Neither the name of the copyright holder(s) nor the names of any
// contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.

// Based on LayoutTests/fast/js/resources/Array-isArray.js

assertTrue(Array.isArray([]));
assertTrue(Array.isArray(new Array));
assertTrue(Array.isArray(Array()));
assertTrue(Array.isArray('abc'.match(/(a)*/g)));
assertFalse((function(){ return Array.isArray(arguments); })());
assertFalse(Array.isArray());
assertFalse(Array.isArray(null));
assertFalse(Array.isArray(undefined));
assertFalse(Array.isArray(true));
assertFalse(Array.isArray(false));
assertFalse(Array.isArray('a string'));
assertFalse(Array.isArray({}));
assertFalse(Array.isArray({length: 5}));
assertFalse(Array.isArray({__proto__: Array.prototype, length:1, 0:1, 1:2}));

