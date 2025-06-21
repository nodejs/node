// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

{
  function a(){}
  function b(){}
  function c(){}
  function d(){}
  function e(){}
  function f(){}
  function g(){}
  function h(){}
}

var names = Object.getOwnPropertyNames(this);
names = names.filter(n => Array.prototype.includes.call("abcdefgh", n));
assertEquals("a,b,c,d,e,f,g,h", names.join());

{
  {
    let j;
    {
      // This j will not be hoisted
      function j(){}
    }
  }
  function i(){}

  // but this j will be.
  function j(){}
}

var names = Object.getOwnPropertyNames(this);
names = names.filter(n => Array.prototype.includes.call("ij", n));
assertEquals("i,j", names.join());
