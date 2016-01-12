//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

"use strict"
var indirect = eval;
indirect("var _et_ = 10;");
WScript.Echo(_et_);
WScript.Echo(eval("delete this._et_;"));
WScript.Echo(typeof _et_);

(function (global) {
    indirect("var _et_ = 10;");
    WScript.Echo(Object.getOwnPropertyDescriptor(global, "_et_").configurable); //Configurability is false for Chakra and true for Chrome & Firefox
    WScript.Echo(delete global._et_);
})(this);
