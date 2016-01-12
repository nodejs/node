//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

baseObj = {
            get val() { return this.value },
            set val(arg) {
                // Recursive call to the setter. Validates inline cache associated with this call site
                if (this.parent) this.parent.val = 2*arg;
                this.value=arg; WScript.Echo("in the setter with " + arg);
                }
          };

function F(v, p)
{
    this.value = v;
    this.parent = p;
}
F.prototype = baseObj;

a = new F(20, null);
b = new F(22, a);
c = new F(24, b);

WScript.Echo(a.val + " " + b.val + " " + c.val);
c.val = 46;
WScript.Echo(a.val + " " + b.val + " " + c.val);
b.val = 13;
WScript.Echo(a.val + " " + b.val + " " + c.val);
