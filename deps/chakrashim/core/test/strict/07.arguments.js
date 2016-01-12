//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

var scenario = [
    ["Assign to Arguments", "arguments=1"],
    ["Post ++   Arguments", "arguments++"],
    ["Post --   Arguments", "arguments--"],
    ["Pre  ++   Arguments", "++arguments"],
    ["Pre  --   Arguments", "--arguments"]
];

var count = 0;

(function test1() {
    write("Changing Arguments...");

    for (var i=0;i<scenario.length;++i) {
        var str = "function f" + i + "() { " + scenario[i][1] + "; }";
        try {
            eval(str);
        } catch (e) {
            write("Exception: " + str + " :: " + scenario[i][0]);
            continue;
        }
        write("Return: " + str + " :: " + scenario[i][0]);
    }
})();
