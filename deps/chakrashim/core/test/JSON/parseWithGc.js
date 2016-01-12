//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Test case for Blue bug 379253
// Construct a json object string with the given number of properties
function GetJSONString(prefix, count)
{
    var buffer = [];    
    for (var i = 0; i < count; i++) {
        buffer.push('"' + prefix + i + '": true');
    }

    return "{ " + buffer.join(',') + " }";
}

var string1 = GetJSONString("prop", 100);
var string2 = GetJSONString("drop", 550);

// Create a JSON object with a 100 properties
var object1 = JSON.parse(string1);

// Clear reference to that object to make its properties eligible for collection
object1 = null;

// Parse a second JSON object, this time with a large number of properties
// This parse has a reviver passed in too to cause an enumeration to occur after parse
var k = 0;
var object2 = JSON.parse(string2, function(key, value) { return k++; });

WScript.Echo("pass");