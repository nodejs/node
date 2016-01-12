//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var typedArrays = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array,
        Uint32Array, Float32Array, Float64Array];
for (var single in typedArrays) {
    testSet(typedArrays[single]);
}

function testSet(single) {
    WScript.Echo("testing" + single);
    testSetObject(single);
    testSetTypedArray(single);
    testSetZero(single);
}

function testSetObject(single) {
    var offsets = [0, 1, 2, 3, 4, 5, 6];
    var nums = [1, 2, 3, 4];
    for (offset in offsets) {
        WScript.Echo("begin offset = " + offset);
        var destArr = new single(10);
        destArr.set(nums, offset)
        for (i = 0; i < destArr.length; i++) {
            WScript.Echo("index " + i + " = " + destArr[i]);
        }
    }
    exceptionThrown = false;
    try {
        destArr.set(nums, 7);
    } catch (e) {
        exceptionThrown = true;
        WScript.Echo("succeeded to throw" + e);
    }
    if (!exceptionThrown)
        WScript.Echo("test failed to throw exception accessing offbound field");
}

function testSetTypedArray(single) {
    for (srcType in typedArrays) {
        WScript.Echo("testing: " + single.toString() + "with source:" + typedArrays[srcType]);
        var offsets = [0, 1, 2, 3, 4, 5, 6];
        var nums = new typedArrays[srcType](4);
        for (var i = 0; i < 4; i++)
            nums[i] = i + 1;
        for (offset in offsets) {
            WScript.Echo("begin offset = " + offset);
            var destArr = new single(10);
            destArr.set(nums, offset)
            for (i = 0; i < destArr.length; i++) {
                WScript.Echo("index " + i + " = " + destArr[i]);
            }
        }
        exceptionThrown = false;
        try {
            destArr.set(nums, 7);
        } catch (e) {
            exceptionThrown = true;
            WScript.Echo("succeeded to throw" + e);
        }
        if (!exceptionThrown)
            WScript.Echo("test failed to throw exception accessing offbound field");
        
        // Test overlap
        WScript.Echo("testing overlap:");
        for (var j = 0; j < nums.length; j++)
          WScript.Echo("index " + j + " = " + nums[j]);
        for (var i = 3; i >= 0; i--) {
          WScript.Echo("overlap offset = " + i);
          nums.set(nums.subarray(i));
          for (var j = 0; j < nums.length; j++)
            WScript.Echo("index " + j + " = " + nums[j]);
        }
    }
}

function testSetZero(single) {
    var a = new single(new ArrayBuffer(0), 0, 0);
    WScript.Echo("a.length = " + a.length);
    for (i = 0; i < a.length; i++) {
        WScript.Echo("index " + i + " = " + a[i]);
    }
}

function testSameArrayBuffer() {
  WScript.Echo('Testing two different types with the same arraybuffer');
  var buffer = new ArrayBuffer(4);
  var int8array = new Int8Array(buffer);
  var int16array = new Int16Array(buffer);
  int16array[0] = 123;
  int16array[1] = 456;
  int8array[3] = 100;
  for (i = 0; i < int8array.length; i++) {
    WScript.Echo("int8array[" + i + "] is " + int8array[i]);
  }
  for (i = 0; i < int16array.length; i++) {
    WScript.Echo("int16array[" + i + "] is " + int16array[i]);
  }
  int16array.set(int8array.subarray(0,1),1);
  for (i = 0; i < int8array.length; i++) {
    WScript.Echo("int8array[" + i + "] is " + int8array[i]);
  }
  for (i = 0; i < int16array.length; i++) {
    WScript.Echo("int16array[" + i + "] is " + int16array[i]);
  }
}
testSameArrayBuffer();