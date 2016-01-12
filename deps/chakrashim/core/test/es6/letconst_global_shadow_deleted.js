//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

this.x = 'global x';
WScript.Echo(x);
delete x;
WScript.Echo(this.x);

this.y = 'global y';
WScript.Echo(y);
delete y;
WScript.Echo(this.y);

WScript.LoadScriptFile('letconst_global_shadow_deleted_2.js');
