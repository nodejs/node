//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try {
    eval("((x = this));");
} catch(ex) {
}

try {
    // This will throw an exception.
    eval("(524288 += x);");
} catch(ex) {
}

WScript.Echo("DONE");
