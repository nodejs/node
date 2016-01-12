//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try { eval("const x = 1; x = 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x += 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x -= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x *= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x /= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x &= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x |= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x ^= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x >>= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x <<= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x >>>= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x %= 2;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x ++;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; x --;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; ++ x;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; -- x;"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; {x++;}"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; {let x = 2; x++;}"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; {x++; let x = 2;}"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; {let x = 2;} x = 10"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; {const x = 2; x++;}"); } catch (e) { WScript.Echo(e); }
try { eval("const x = 1; {const x = 2;} x++;"); } catch (e) { WScript.Echo(e); }
try { eval("x = 1; {let x = 2;} const x = 10;"); } catch (e) { WScript.Echo(e); }
try { eval("function f() {x = 1; {let x = 2;} const x = 10;} f();"); } catch (e) { WScript.Echo(e); }


