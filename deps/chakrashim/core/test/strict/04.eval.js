//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var _myEval = eval;

var scenario = [
    ["Assign to Eval", "eval = 1;"],
    ["Post ++   Eval", "eval++;"],
    ["Post --   Eval", "eval--;"],
    ["Pre  ++   Eval", "++eval;"],
    ["Pre  --   Eval", "--eval;"]
];

var count = 0;

(function test1() {
    write("Changing eval...");

    for (var i=0;i<scenario.length;++i) {
        try {
            _myEval(scenario[i][1]);
        } catch (e) {
            write("Exception: " + scenario[i][0]);
            continue;
        }
        write("Return: " + scenario[i][0]);
    }
})();
