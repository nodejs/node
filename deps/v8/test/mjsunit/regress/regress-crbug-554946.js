// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var array = [];
var funky = {
  toJSON: function() { array.length = 1; return "funky"; }
};
for (var i = 0; i < 10; i++) array[i] = i;
array[0] = funky;
assertEquals('["funky",null,null,null,null,null,null,null,null,null]',
             JSON.stringify(array));
