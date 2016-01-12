//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function foo() {
    "use strict";

    // --- invalid statements ---
    // (eval)++;
    // ++(eval);
    // (arguments)++;
    // ++(arguments);
    //
    // (eval) = 1;
    // (arguments) = 1;

    // --- valid statements ---
    eval[0]++;
    ++eval[0];
    eval.a++;
    ++eval.a;
    arguments[0]++;
    ++arguments[0];
    arguments.a++;
    ++arguments.a;

    eval[0] = 1;
    arguments[0] = 1;
    eval.a = 1;
    arguments.a = 1;
}

foo();
