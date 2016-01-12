//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v); }

write(null);
write(undefined);
write(10);
write(1.24);
write(true);
write(false);
var o = new Object()
write(o);

write(Object.prototype.toString.apply(null));
write(Object.prototype.toString.apply(undefined));