//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function write(v) { WScript.Echo(v + ""); }

function exceptToString(ee) {
    if(ee instanceof TypeError) return "TypeError";
    if(ee instanceof ReferenceError) return "ReferenceError";
    if(ee instanceof EvalError) return "EvalError";
    if(ee instanceof SyntaxError) return "SyntaxError";
    return "Unknown Error";
}

(function () {
    var description = "Program.SourceElement.FunctionDeclaration";

    try {
        eval("function foo() {}");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "FunctionBody.FunctionDeclaration";

    try {
        eval("(function() { function foo() {} });");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "Block.StatementList.Statement.FunctionDeclaration";

    try {
        eval("{ function foo() {} }");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "IfStatement.Statement.FunctionDeclaration";

    try {
        eval("if(false) function foo() {}");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "IterationStatement(do-while).Statement.FunctionDeclaration";

    try {
        eval("do function foo() {} while(false);");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "IterationStatement(while).Statement.FunctionDeclaration";

    try {
        eval("while(false) function foo() {}");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "IterationStatement(for).Statement.FunctionDeclaration";

    try {
        eval("for(var i = 0; i < 0; ++i) function foo() {}");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "IterationStatement(for-in).Statement.FunctionDeclaration";

    try {
        eval("for(var p in {}) function foo() {}");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "SourceElement.Statement.LabelledStatement.Statement.FunctionDeclaration";

    try {
        eval("Foo: function foo() {}");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "Block.Statement.LabelledStatement.Statement.FunctionDeclaration";

    try {
        eval("{ Foo: function foo() {} }");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "SwitchStatement.CaseBlock.CaseClause.StatementList.Statement.FunctionDeclaration";

    try {
        eval("switch(true) { case false: function foo() {} }");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "SwitchStatement.CaseBlock.DefaultClause.StatementList.Statement.FunctionDeclaration";

    try {
        eval("switch(true) { case true: break; default: function foo() {} }");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "SwitchStatement.CaseBlock.DefaultClause.StatementList.Statement.FunctionDeclaration";

    try {
        eval("switch(true) { case true: break; default: function foo() {} }");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "TryStatement.Block.StatementList.Statement.FunctionDeclaration";

    try {
        eval("try { function foo() {} } finally {}");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "TryStatement.Catch.Block.StatementList.Statement.FunctionDeclaration";

    try {
        eval("try {} catch(ex) { function foo() {} }");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();

(function () {
    var description = "TryStatement.Finally.Block.StatementList.Statement.FunctionDeclaration";

    try {
        eval("try {} finally { function foo() {} }");
    } catch(e) {
        write("Exception: " + description + " - " + exceptToString(e));
        return;
    }
    write("Return: " + description);
})();
