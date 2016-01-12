//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

try { var f = Function("const x = 10; return x;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{const x = 10; return x;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{const x = 10; x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; {x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{x = 20;} const x = 10;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{x = 20;} let x = 10;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; function f() {x = 10;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; function f() {let x; x = 10;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; {let x = 1; x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("let x = 10; {const x = 1; x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{const x;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){const x;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; const x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; let x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("let x = 10; const x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("let x = 10; let x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{const x = 10; const x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{const x = 10; let x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{let x = 10; const x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{let x = 10; let x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){const x = 10; const x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){const x = 10; let x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){let x = 10; const x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){let x = 10; let x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("var x = 10; const x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("var x = 10; let x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("const x = 10; var x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("let x = 10; var x = 20;"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{var x = 10; const x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{var x = 10; let x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{const x = 10; var x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("{let x = 10; var x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){var x = 10; const x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){var x = 10; let x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){const x = 10; var x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }
try { var f = Function("function g(){let x = 10; var x = 20;}"); WScript.Echo("Syntax check succeeded"); } catch (e) { WScript.Echo(e); }

