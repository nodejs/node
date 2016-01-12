//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var arrayBuffer = new ArrayBuffer(8);
var floatArray = new Float32Array(arrayBuffer);
var doubleArray = new Float64Array(arrayBuffer);
var floatArray2 = new Float32Array(1);
var doubleArray2 = new Float64Array(1);

function print(data)
{
  try
  {
    WScript.Echo(data);
  }
  catch(e)
  {
    document.write(data);
  }
}

function printTypedArray(typedArray)
{
  for (var i = 0; i < typedArray.length; i++)
  {
    print(i + " == " + typedArray[i]);
  }
}

function printTypedArrayByte(typedArray)
{
var tmp = new Uint8Array(typedArray.buffer);
printTypedArray(tmp);
}

function setBit(obj, bitCount, value, isDouble)
{
  if (bitCount > 32)
  {
    throw "only support setting less than 16 bits at this time";
  }
  var currentBit = 1 << 31;
  var current = 0;
  for (var i = 0; i < bitCount; i++)
  {
    currentBit = 1 << (31-i);
    current = current | currentBit;
//    currentBit = currentBit >> 1;
  }
  var tmp = new Uint32Array(obj.buffer);
  if (isDouble)
  {
    tmp[1] = current;
  }
  else
  {
    tmp[0] = current;
  }
}

function printOneSet(typedArray, backup)
{
  print("original value");
  printTypedArrayByte(typedArray);
  print(typedArray[0]);
  print("after assign to separate typed array");
  printTypedArrayByte(backup);
  print(backup[0]);
}

function testOneNan(typedArray, backup, isDouble)
{
  print("set NaN");
  typedArray[0] = NaN;
  print(typedArray[0]);
  printTypedArrayByte(typedArray);
  typedArray[0] = 0;

  print("set 8 bits");
  setBit(typedArray, 10, 1, isDouble);
  backup[0] = typedArray[0];
  printOneSet(typedArray, backup);

  for (var j = 12 ; j < 20; j++)
  {
    print("set " + j + " bits");
    setBit(typedArray, j, 1, isDouble);
    backup[0] = typedArray[0];
    printOneSet(typedArray, backup);
  }

}

print("test float");
testOneNan(floatArray, floatArray2, false);
print("test double");
testOneNan(doubleArray, doubleArray2, true);