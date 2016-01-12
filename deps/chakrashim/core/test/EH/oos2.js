//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function echo(m) { this.WScript ? WScript.Echo(m) : console.log(m); }

function oos() {
    oos();
}

try {
    try {
        oos();
    } finally {
        try {
            oos();
        } catch (e) {
        } finally {
        }
    }
    //
    // Win8: 772949
    //      The inner finally cleared threadContext->OOS.thrownObject.
    //
    //      In chk build, outer finally asserts.
    //      In fre build, outer finally gets a NULL thrownObject from shared OOS and sends
    //          NULL to catch below. e == NULL, causes AV as NULL is not a valid Var.
    //
} catch (e) {
    if (e) {
        echo("pass");
    }
}
