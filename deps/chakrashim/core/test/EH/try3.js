//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var e = 8;
function x() { throw 7; }
function y() {
    var i;
    for (i = 0; i < 10; i++) {
        try {
            if (i % 2 == 0) {
                try {
                    x();
                }
                catch (e) {
                    WScript.Echo("Inner catch: " + e);
                    if (i % 3) {
                        throw e;
                    }
                    if (i % 5) {
                        return e;
                    }
                }
                finally {
                    WScript.Echo("Finally: " + i);
                    continue;
                }
            }
        }
        catch (e) {
            WScript.Echo("Outer catch: " + e);
        }
        finally {
            WScript.Echo("Outer finally: " + i);
            if (++i % 9 == 0)
                return e;
        }
    }
}

WScript.Echo(y());