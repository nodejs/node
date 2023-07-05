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

// Flags: --allow-natives-syntax

// Check that keyed stores make things go dict mode faster than non-keyed
// stores.

function AddProps(obj) {
  for (var i = 0; i < 26; i++) {
    obj["x" + i] = 0;
  }
}


function AddPropsNonKeyed(obj) {
  obj.x0 = 0;
  obj.x1 = 0;
  obj.x2 = 0;
  obj.x3 = 0;
  obj.x4 = 0;
  obj.x5 = 0;
  obj.x6 = 0;
  obj.x7 = 0;
  obj.x8 = 0;
  obj.x9 = 0;
  obj.x10 = 0;
  obj.x11 = 0;
  obj.x12 = 0;
  obj.x13 = 0;
  obj.x14 = 0;
  obj.x15 = 0;
  obj.x16 = 0;
  obj.x17 = 0;
  obj.x18 = 0;
  obj.x19 = 0;
  obj.x20 = 0;
  obj.x21 = 0;
  obj.x22 = 0;
  obj.x23 = 0;
  obj.x24 = 0;
  obj.x25 = 0;
}

function AddProps3(obj) {
  obj["x0"] = 0;
  obj["x1"] = 0;
  obj["x2"] = 0;
  obj["x3"] = 0;
  obj["x4"] = 0;
  obj["x5"] = 0;
  obj["x6"] = 0;
  obj["x7"] = 0;
  obj["x8"] = 0;
  obj["x9"] = 0;
  obj["x10"] = 0;
  obj["x11"] = 0;
  obj["x12"] = 0;
  obj["x13"] = 0;
  obj["x14"] = 0;
  obj["x15"] = 0;
  obj["x16"] = 0;
  obj["x17"] = 0;
  obj["x18"] = 0;
  obj["x19"] = 0;
  obj["x20"] = 0;
  obj["x21"] = 0;
  obj["x22"] = 0;
  obj["x23"] = 0;
  obj["x24"] = 0;
  obj["x25"] = 0;
}


var keyed = {};
AddProps(keyed);
assertFalse(%HasFastProperties(keyed));

var non_keyed = {};
AddPropsNonKeyed(non_keyed);
assertTrue(%HasFastProperties(non_keyed));

var obj3 = {};
AddProps3(obj3);
assertTrue(%HasFastProperties(obj3));

var funny_name = {};
funny_name[".foo"] = 0;
assertTrue(%HasFastProperties(funny_name));
