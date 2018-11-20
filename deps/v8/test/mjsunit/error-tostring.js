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


// Test default string representation of an Error object.

var e = new Error();
assertEquals('Error', e.toString());


// Test printing of cyclic errors which return the empty string for
// compatibility with Safari and Firefox.

e = new Error();
e.name = e;
e.message = e;
e.stack = "Does not occur in output";
e.arguments = "Does not occur in output";
e.type = "Does not occur in output";
assertThrows(()=>e.toString(), RangeError);

e = new Error();
e.name = [ e ];
e.message = [ e ];
e.stack = "Does not occur in output";
e.arguments = "Does not occur in output";
e.type = "Does not occur in output";
assertEquals('', e.toString());


// Test the sequence in which getters and toString operations are called
// on a given Error object.  Verify the produced string representation.

function testErrorToString(nameValue, messageValue) {
  var seq = [];
  var e = {
    get name() {
      seq.push(1);
      return (nameValue === undefined) ? nameValue : {
        toString: function() { seq.push(2); return nameValue; }
      };
    },
    get message() {
      seq.push(3);
      return (messageValue === undefined) ? messageValue : {
        toString: function() { seq.push(4); return messageValue; }
      };
    }
  };
  var string = Error.prototype.toString.call(e);
  return [string,seq];
}

assertEquals(["Error",[1,3]], testErrorToString(undefined, undefined));
assertEquals(["e1",[1,2,3]], testErrorToString("e1", undefined));
assertEquals(["e1: null",[1,2,3,4]], testErrorToString("e1", null));
assertEquals(["e1",[1,2,3,4]], testErrorToString("e1", ""));
assertEquals(["Error: e2",[1,3,4]], testErrorToString(undefined, "e2"));
assertEquals(["null: e2",[1,2,3,4]], testErrorToString(null, "e2"));
assertEquals(["e2",[1,2,3,4]], testErrorToString("", "e2"));
assertEquals(["e1: e2",[1,2,3,4]], testErrorToString("e1", "e2"));

var obj = {
  get constructor () {
    assertUnreachable();
  }
};

assertThrows(function() { obj.x(); });
