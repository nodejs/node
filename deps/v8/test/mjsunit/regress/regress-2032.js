// Copyright 2012 the V8 project authors. All rights reserved.
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

// See: http://code.google.com/p/v8/issues/detail?id=2032

// Case independent regexp that ends on the first character in a block.
assertTrue(/[@-A]/i.test("a"));
assertTrue(/[@-A]/i.test("A"));
assertTrue(/[@-A]/i.test("@"));

assertFalse(/[@-A]/.test("a"));
assertTrue(/[@-A]/.test("A"));
assertTrue(/[@-A]/.test("@"));

assertFalse(/[¿-À]/i.test('¾'));
assertTrue(/[¿-À]/i.test('¿'));
assertTrue(/[¿-À]/i.test('À'));
assertTrue(/[¿-À]/i.test('à'));
assertFalse(/[¿-À]/i.test('á'));
assertFalse(/[¿-À]/i.test('Á'));

assertFalse(/[¿-À]/.test('¾'));
assertTrue(/[¿-À]/.test('¿'));
assertTrue(/[¿-À]/.test('À'));
assertFalse(/[¿-À]/.test('à'));
assertFalse(/[¿-À]/.test('á'));
assertFalse(/[¿-À]/.test('á'));
assertFalse(/[¿-À]/i.test('Á'));

assertFalse(/[Ö-×]/i.test('Õ'));
assertTrue(/[Ö-×]/i.test('Ö'));
assertTrue(/[Ö-×]/i.test('ö'));
assertTrue(/[Ö-×]/i.test('×'));
assertFalse(/[Ö-×]/i.test('Ø'));

assertFalse(/[Ö-×]/.test('Õ'));
assertTrue(/[Ö-×]/.test('Ö'));
assertFalse(/[Ö-×]/.test('ö'));
assertTrue(/[Ö-×]/.test('×'));
assertFalse(/[Ö-×]/.test('Ø'));
