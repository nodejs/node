//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test2() {
    x = 10;
}
try { test2(); } catch (e) { print(e); }

Object.defineProperty(this, 'accessor', { configurable: true, get: function () { return false; } });

try { test2(); } catch (e) { print(e); }
