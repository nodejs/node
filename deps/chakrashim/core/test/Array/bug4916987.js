//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//Baseline switches:
//Switches: -mic:1 -off:simplejit
var Float64ArrayView = new Float64Array();
var Int32ArrayView = new Int32Array();

function m(v) {
  Float64ArrayView[0x4 * (0x80000001 >> !1) >> 0] = v;
  Int32ArrayView[0x4 * (0x80000001 >> !1) >> 0] = v;
}

var val = 3.1415926535;
m(val);
val = 123456789.123456789;
m(val);

Float64ArrayView = new Float64Array(16);
Int32ArrayView = new Int32Array(16);
val = 987654321.987654321;
m(val);
if (Float64ArrayView[4] === val && Int32ArrayView[4] === (val | 0)) {
  print("PASSED");
} else {
  print(Float64ArrayView[4]);
  print(Int32ArrayView[4]);
  print("FAILED");
}
