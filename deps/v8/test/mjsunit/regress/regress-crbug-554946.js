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

array = [];
funky = {
  get value() { array.length = 1; return "funky"; }
};
for (var i = 0; i < 10; i++) array[i] = i;
array[3] = funky;
assertEquals('[0,1,2,{"value":"funky"},null,null,null,null,null,null]',
             JSON.stringify(array));

array = [];
funky = {
  get value() { array.pop(); return "funky"; }
};
for (var i = 0; i < 10; i++) array[i] = i;
array[3] = funky;
assertEquals('[0,1,2,{"value":"funky"},4,5,6,7,8,null]', JSON.stringify(array));

array = [];
funky = {
  get value() { delete array[9]; return "funky"; }
};
for (var i = 0; i < 10; i++) array[i] = i;
array[3] = funky;
assertEquals('[0,1,2,{"value":"funky"},4,5,6,7,8,null]', JSON.stringify(array));

array = [];
funky = {
  get value() { delete array[6]; return "funky"; }
};
for (var i = 0; i < 10; i++) array[i] = i;
array[3] = funky;
assertEquals('[0,1,2,{"value":"funky"},4,5,null,7,8,9]', JSON.stringify(array));

array = [];
funky = {
  get value() { array[12] = 12; return "funky"; }
};
for (var i = 0; i < 10; i++) array[i] = i;
array[3] = funky;
assertEquals('[0,1,2,{"value":"funky"},4,5,6,7,8,9]', JSON.stringify(array));

array = [];
funky = {
  get value() { array[10000000] = 12; return "funky"; }
};
for (var i = 0; i < 10; i++) array[i] = i;
array[3] = funky;
assertEquals('[0,1,2,{"value":"funky"},4,5,6,7,8,9]', JSON.stringify(array));
