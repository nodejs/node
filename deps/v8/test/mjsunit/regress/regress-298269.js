// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function Cb(a, trigger) {
  var f, g;
  for(f = a.length; f--;) {
    g = a.charCodeAt(f);
    // This will fail after OSR if Runtime_StringCharCodeAt is modified
    // to iterates optimized frames and visit safepoint pointers.
    if (g == "C".charCodeAt(0)) {
      %OptimizeOsr();
      %PrepareFunctionForOptimization(Cb);
    }
  }
  return g;
}

var s1 = "long string to make cons string 1";
var s2 = "long string to make cons string 2";
%PrepareFunctionForOptimization(Cb);
Cb(s1 + s2);
%PrepareFunctionForOptimization(Cb);
Cb(s1);
var s3 = "string for triggering osr in Cb";
%PrepareFunctionForOptimization(Cb);
Cb(s3 + s3);
%PrepareFunctionForOptimization(Cb);
Cb(s1 + s2);
