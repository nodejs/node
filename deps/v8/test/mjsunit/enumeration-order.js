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

function check_enumeration_order(obj)  {
  var value = 0; 
  for (var name in obj) assertTrue(value < obj[name]);
  value = obj[name];
}

function make_object(size)  {
  var a = new Object();
  
  for (var i = 0; i < size; i++) a["a_" + i] = i + 1;
  check_enumeration_order(a);
  
  for (var i = 0; i < size; i +=3) delete a["a_" + i];
  check_enumeration_order(a);
}

// Validate the enumeration order for object up to 100 named properties.
for (var j = 1; j< 100; j++) make_object(j);


function make_literal_object(size)  {
  var code = "{ ";
  for (var i = 0; i < size-1; i++) code += " a_" + i + " : " + (i + 1) + ", ";
  code += "a_" + (size - 1) + " : " + size;
  code += " }";
  eval("var a = " + code);
  check_enumeration_order(a);  
}

// Validate the enumeration order for object literals up to 100 named properties.
for (var j = 1; j< 100; j++) make_literal_object(j);

