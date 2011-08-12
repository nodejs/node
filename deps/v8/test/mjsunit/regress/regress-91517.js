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

// Getting property names of an object with a prototype chain that
// triggers dictionary elements in GetLocalPropertyNames() shouldn't
// crash the runtime

// Flags: --allow-natives-syntax

function Object1() {
  this.foo = 1;
}

function Object2() {
  this.fuz = 2;
  this.objects = new Object();
  this.fuz1 = 2;
  this.fuz2 = 2;
  this.fuz3 = 2;
  this.fuz4 = 2;
  this.fuz5 = 2;
  this.fuz6 = 2;
  this.fuz7 = 2;
  this.fuz8 = 2;
  this.fuz9 = 2;
  this.fuz10 = 2;
  this.fuz11 = 2;
  this.fuz12 = 2;
  this.fuz13 = 2;
  this.fuz14 = 2;
  this.fuz15 = 2;
  this.fuz16 = 2;
  this.fuz17 = 2;
  // Force dictionary-based properties
  for (x=1;x<1000;x++) {
    this["sdf" + x] = 2;
  }
}

function Object3() {
  this.boo = 3;
}

function Object4() {
  this.baz = 4;
}

obj1 = new Object1();
obj2 = new Object2();
obj3 = new Object3();
obj4 = new Object4();

%SetHiddenPrototype(obj4, obj3);
%SetHiddenPrototype(obj3, obj2);
%SetHiddenPrototype(obj2, obj1);

function contains(a, obj) {
  for(var i = 0; i < a.length; i++) {
    if(a[i] === obj){
      return true;
    }
  }
  return false;
}
names = %GetLocalPropertyNames(obj4);
assertEquals(1021, names.length);
assertTrue(contains(names, "baz"));
assertTrue(contains(names, "boo"));
assertTrue(contains(names, "foo"));
assertTrue(contains(names, "fuz"));
assertTrue(contains(names, "fuz1"));
assertTrue(contains(names, "fuz2"));
assertTrue(contains(names, "fuz3"));
assertTrue(contains(names, "fuz4"));
assertTrue(contains(names, "fuz5"));
assertTrue(contains(names, "fuz6"));
assertTrue(contains(names, "fuz7"));
assertTrue(contains(names, "fuz8"));
assertTrue(contains(names, "fuz9"));
assertTrue(contains(names, "fuz10"));
assertTrue(contains(names, "fuz11"));
assertTrue(contains(names, "fuz12"));
assertTrue(contains(names, "fuz13"));
assertTrue(contains(names, "fuz14"));
assertTrue(contains(names, "fuz15"));
assertTrue(contains(names, "fuz16"));
assertTrue(contains(names, "fuz17"));
assertFalse(names[1020] == undefined);
