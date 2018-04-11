// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(() => {

benchy('EmptyArrayOf', EmptyArrayOf, EmptyArrayOfSetup);
benchy('SmallTransplantedArrayOf', SmallTransplantedArrayOf,
    SmallTransplantedArrayOfSetup);
benchy('SmallSmiArrayOf', SmallSmiArrayOf, SmallSmiArrayOfSetup);
benchy('LargeSmiArrayOf', LargeSmiArrayOf, LargeSmiArrayOfSetup);
benchy('SmallDoubleArrayOf', SmallDoubleArrayOf, SmallDoubleArrayOfSetup);
benchy('SmallStringArrayOf', SmallStringArrayOf, SmallStringArrayOfSetup);
benchy('SmallMixedArrayOf', SmallMixedArrayOf, SmallMixedArrayOfSetup);

function ArrayLike() {}
ArrayLike.of = Array.of;

var arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10
var arg11, arg12, arg13, arg14, arg15, arg16, arg17, arg18, arg19, arg20
var result;

function EmptyArrayOf() {
  result = Array.of();
}

function BaselineArray() {
  result = [arg1, arg2, arg3];
}

function SmallSmiArrayOf() {
  result = Array.of(arg1, arg2, arg3);
}

function LargeSmiArrayOf() {
  result = Array.of(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10,
      arg11, arg12, arg13, arg14, arg15, arg16, arg17, arg18, arg19, arg20);
}

function SmallTransplantedArrayOf() {
  result = ArrayLike.of(arg1, arg2, arg3);
}

function SmallDoubleArrayOf() {
  result = Array.of(arg1, arg2, arg3);
}

function SmallStringArrayOf() {
  result = Array.of(arg1, arg2, arg3);
}

function SmallMixedArrayOf() {
  result = Array.of(arg1, arg2, arg3);
}

function EmptyArrayOfSetup() {
}

function BaselineArraySetup() {
  arg1 = 1;
  arg2 = 2;
  arg3 = 3;
}

function SmallSmiArrayOfSetup() {
  arg1 = 1;
  arg2 = 2;
  arg3 = 3;
}

function SmallTransplantedArrayOfSetup() {
  arg1 = 1;
  arg2 = 2;
  arg3 = 3;
}

function SmallDoubleArrayOfSetup() {
  arg1 = 1.5;
  arg2 = 2.5;
  arg3 = 3.5;
}

function SmallStringArrayOfSetup() {
  arg1 = "cat";
  arg2 = "dog";
  arg3 = "giraffe";
}

function SmallMixedArrayOfSetup() {
  arg1 = 1;
  arg2 = 2.5;
  arg3 = "giraffe";
}

function LargeSmiArrayOfSetup() {
  arg1 = 1;
  arg2 = 2;
  arg3 = 3;
  arg4 = 4;
  arg5 = 5;
  arg6 = 6;
  arg7 = 7;
  arg8 = 8;
  arg9 = 9;
  arg10 = 10;
  arg11 = 11;
  arg12 = 12;
  arg13 = 13;
  arg14 = 14;
  arg15 = 15;
  arg16 = 16;
  arg17 = 17;
  arg18 = 18;
  arg19 = 19;
  arg20 = 20;
}

})();
