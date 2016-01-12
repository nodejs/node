//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

for (a in this) {
    if (a === "SCA" || a === "ImageData")
        continue;
    WScript.Echo(a);
}


const x = 10;
WScript.Echo(x);
{
    const x = 20;
    WScript.Echo(x);
}
WScript.Echo(x);



for (a in this) {
    if (a === "SCA" || a === "ImageData")
        continue;
    WScript.Echo(a);
}

