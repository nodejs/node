//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var go = { foo: 1, print: function () { } };
function test1() {
    for (var i = 0; i < 2; ++i) {
        if (i % 2 == 1) {
            go.print();
            x = go.foo;
        } else {
            arguments = 1;

        }

    };

    WScript.Echo(arguments);
};

test1();
go.bar = 1;

test1();
