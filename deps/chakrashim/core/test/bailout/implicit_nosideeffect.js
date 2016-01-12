//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


function print(s) {}
function f(a) {
    for (var vxmvvw = 0; vxmvvw < 1; ++vxmvvw) { if (vxmvvw % 10 == 3) { print(x); } else 
    { ( eval('""  <<= a') ); }  } 
}
function Ctor()
{
}

Ctor.prototype.toString = Number.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = RegExp.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = Function.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = Object.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = Error.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = Boolean.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = Array.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = String.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.toString = Date.prototype.toString;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }

Ctor.prototype = new Object();
Ctor.prototype.valueOf = Boolean.prototype.valueOf;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.valueOf = Date.prototype.valueOf;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.valueOf = Number.prototype.valueOf;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.valueOf = Object.prototype.valueOf;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }
Ctor.prototype.valueOf = String.prototype.valueOf;
try { f(new Ctor()); } catch (e) { WScript.Echo(e); }



