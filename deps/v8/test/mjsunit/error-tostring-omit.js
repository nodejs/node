// Copyright 2013 the V8 project authors. All rights reserved.
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

function veryLongString() {
  return "Lorem ipsum dolor sit amet, consectetur adipiscing elit." +
         "Nam vulputate metus est. Maecenas quis pellentesque eros," +
         "ac mattis augue. Nam porta purus vitae tincidunt blandit." +
         "Aliquam lacus dui, blandit id consectetur id, hendrerit ut" +
         "felis. Class aptent taciti sociosqu ad litora torquent per" +
         "conubia nostra, per inceptos himenaeos. Ut posuere eros et" +
         "tempus luctus. Nullam condimentum aliquam odio, at dignissim" +
         "augue tincidunt in. Nam mattis vitae mauris eget dictum." +
         "Nam accumsan dignissim turpis a turpis duis.";
}


var re = /omitted/;

try {
  veryLongString.nonexistentMethod();
} catch (e) {
  assertTrue(e.message.length < 350);
  // TODO(verwaest): Proper error message.
  // assertTrue(re.test(e.message));
}

try {
  veryLongString().nonexistentMethod();
} catch (e) {
  assertTrue(e.message.length < 350);
  // TODO(verwaest): Proper error message.
  // assertTrue(re.test(e.message));
}

try {
  throw Error(veryLongString());
} catch (e) {
  assertEquals(veryLongString(), e.message);
}
