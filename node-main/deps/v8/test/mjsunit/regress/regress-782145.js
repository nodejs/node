// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function newFastRegExp() { return new RegExp('.'); }
function toSlowRegExp(re) { re.exec = 42; }

let re = newFastRegExp();
const evil_nonstring = { [Symbol.toPrimitive]: () => toSlowRegExp(re) };
const empty_string = "";

String.prototype.replace.call(evil_nonstring, re, empty_string);

re = newFastRegExp();
String.prototype.match.call(evil_nonstring, re, empty_string);

re = newFastRegExp();
String.prototype.search.call(evil_nonstring, re, empty_string);

re = newFastRegExp();
String.prototype.split.call(evil_nonstring, re, empty_string);
