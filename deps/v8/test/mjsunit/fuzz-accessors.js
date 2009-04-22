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

var builtInPropertyNames = [
  'prototype', 'length', 'caller', 0, 1, '$1', 'arguments', 'name', 'message', 'constructor'
];

function getAnException() {
  try {
    ("str")();
  } catch (e) {
    return e;
  }
}

function getSpecialObjects() {
  return [
    function () { },
    [1, 2, 3],
    /xxx/,
    RegExp,
    "blah",
    9,
    new Date(),
    getAnException()
  ];
}

var object = { };
var fun = function () { };
var someException = getAnException();
var someDate = new Date();

var objects = [
  [1, Number.prototype],
  ["foo", String.prototype],
  [true, Boolean.prototype],
  [object, object],
  [fun, fun],
  [someException, someException],
  [someDate, someDate]
];

function runTest(fun) {
  for (var i in objects) {
    var obj = objects[i][0];
    var chain = objects[i][1];
    var specialObjects = getSpecialObjects();
    for (var j in specialObjects) {
      var special = specialObjects[j];
      chain.__proto__ = special;
      for (var k in builtInPropertyNames) {
        var propertyName = builtInPropertyNames[k];
        fun(obj, propertyName);
      }
    }
  }
}

runTest(function (obj, name) { return obj[name]; });
runTest(function (obj, name) { return obj[name] = { }; });
