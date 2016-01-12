//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var getFuncs = ['getInt8', 'getUint8', 'getInt16', 'getUint16', 'getInt32', 'getUint32', 'getFloat32', 'getFloat64'];
var setFuncs = ['setInt8', 'setUint8', 'setInt16', 'setUint16', 'setInt32', 'setUint32', 'setFloat32', 'setFloat64'];
var dataSize = [1,1,2,2,4,4,4,8];
var testValue = [1, -1, 20, -20, 0x128];

if (dataSize.length != setFuncs.length)
{
throw new TypeError("testerror: invalid test data");
}
if (getFuncs.length != setFuncs.length)
{
throw new TypeError("testerror: invalid test data");
}

if (typeof(print) == "undefined")
{
    function print(data)
    {
        WScript.Echo(data);
    }
}

function printDataView(dataView)
{
    print(dataView.toString());
    for (i in dataView)
    {
        print(i + " == " + dataView[i]);
    };
    print("array content");
    var tmp = new Int8Array(dataView.buffer);
    for (i in tmp)
    {
        print(i + " == " + dataView[i]);
    }
}

var arrayBuffer = new ArrayBuffer(16);
var dataView = new DataView(arrayBuffer);

function GetResult(dataView, methodId, offset, isLittleEndian)
{
var res;
var shouldThrow= false;
var thrown = false;
        if (offset + dataSize[methodId] > dataView.byteLength)
            {
            shouldThrow = true;
            }
        
            var thrown = false;
            try {
                res = dataView[getFuncs[methodId]](offset, isLittleEndian);
                }
            catch(e)
                {
                thrown = true;
                }
            if (shouldThrow & !thrown)
                {
                throw Error("failed to throw for out of bound access");
                }
return res;
}
function testOneOffset(dataView, offSet, value)
{
    for (var i = 0; i < setFuncs.length; i++)
        {
        if (offSet + dataSize[i] > dataView.byteLength)
            {
            var succeeded = false;
            try {
                dataView[setFuncs[i]](offSet, value);
                }
            catch(e)
                {
                print("SUCCEEDED: exception " + e.description);
                succeeded = true;
                }
            if (!succeeded)
                {
                print("failed to throw for out of bound access " + setFuncs[i] + " dataOffset is " + offSet);
                }
            }
        else
            {
            print("set little endian value offset " + offSet + " value " + value + " method " + setFuncs[i]);
            print("results of little endian reads are: ");
            dataView[setFuncs[i]](offSet, value, true);
            for (var j = 0; j < getFuncs.length; j++)
                {
                var result = GetResult(dataView, j, offSet, true);
                print(getFuncs[j] + " = " + result);
                }
            print("results of big endian reads are: ");
            for (var j = 0; j < getFuncs.length; j++)
                {
                var result = GetResult(dataView, j, offSet, false);
                print(getFuncs[j] + " = " +  result);
                }

            print("set little endian value offset " + offSet + " value " + value + " method " + setFuncs[i]);
            print("results of little endian reads are: ");
            dataView[setFuncs[i]](offSet, value, false);
            for (var j = 0; j < getFuncs.length; j++)
                {
                var result = GetResult(dataView, j, offSet, true);
                print(getFuncs[j] + " = " +  result);
                }
            print("results of big endian reads are: ");
            for (var j = 0; j < getFuncs.length; j++)
                {
                var result = GetResult(dataView, j, offSet, false);
                print(getFuncs[j] + " = " +  result);
                }
            }

        }
}

function testOneValue(dataView, value)
{
    print("test one value " + value);
    for (var i = 0; i < dataView.byteLength; i++)
    {
        testOneOffset(dataView, i, value);
    }
    for (var i = 0; i < dataView.byteLength; i++) 
        dataView.setInt8(i, 0);
}

function oneTest(dataView)
{
    for (var i = 0; i < testValue.length; i++)
    {
        testOneValue(dataView, i, testValue[i]);
    }
}

oneTest(dataView);
if (Object.getOwnPropertyDescriptor(dataView, 100000) != undefined) {
    WScript.Echo('FAIL');
}

WScript.Echo("prototype test");
WScript.Echo(DataView.prototype[10]);
WScript.Echo(DataView.prototype[-1]);
WScript.Echo(DataView.prototype[2]);
DataView.prototype[2] = 10;
WScript.Echo(DataView.prototype[2]); 
