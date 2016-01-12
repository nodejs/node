//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

let x = 'let x';

WScript.Echo(x);
WScript.Echo(this.x);

this.x = 'global x';

WScript.Echo(x);
WScript.Echo(this.x);

const y = 'const y';

WScript.Echo(y);
WScript.Echo(this.y);

Object.defineProperty(this, 'y', { configurable: true, get: function () { return 'getter'; } });

WScript.Echo(y);
WScript.Echo(this.y);
