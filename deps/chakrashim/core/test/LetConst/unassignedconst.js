//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var passed = true;
try { eval('const x;'); } catch (e) { if (e.message != "Const must be initialized") { passed = false; } }
try { eval('function a() { const x; }'); } catch (e) { if (e.message != "Const must be initialized") { passed = false; } }
WScript.LoadScriptFile('unassignedconst_noneval_global.js');
try { test1(); } catch (e) { if (e.message != "Const must be initialized") { passed = false; }}
WScript.LoadScriptFile('unassignedconst_noneval_function.js');
try { test2(); } catch (e) { if (e.message != "Const must be initialized") { passed = false; }}

if (passed) {
    print('Pass');
}
else {
    print('Fail');
}
