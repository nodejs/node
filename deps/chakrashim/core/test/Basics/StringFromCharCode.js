//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function print(value)
{
    WScript.Echo(value);
}

print(String.fromCharCode(65, 66, 67));
print(String.fromCharCode(65.23, 66, 67.98));
print(String.fromCharCode('65', '66', '67'));
