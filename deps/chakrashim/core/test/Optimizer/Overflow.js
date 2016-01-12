//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var NonwideningOperation = {
    None: 0,
    Neg: 1,
    Ld: 2,
    ConvNum: 3,

    Count: 4
};

var bools = [false, true];

// Test helper constants and variables
var indent = 0;
var nonwideningOperation = -1;

var minTreeDepth = (53 - 32) >> 1;
var maxTreeDepth = (53 - 32 + 1) << 1;
var midTreeDepth = (minTreeDepth + maxTreeDepth) >> 1;
for(var treeDepth = minTreeDepth;
    treeDepth <= maxTreeDepth;
    treeDepth += treeDepth <= midTreeDepth ? treeDepth - (minTreeDepth - 1) : ((maxTreeDepth - treeDepth) >> 1) + 1) {

    for(var treeBranches = 1; treeBranches <= 8; treeBranches <<= 1) {
        for(var useNegativeNumbersIndex = 0; useNegativeNumbersIndex < bools.length; ++useNegativeNumbersIndex) {
            var useNegativeNumbers = bools[useNegativeNumbersIndex];

            echo("generateAndRun(" + treeDepth + ", " + treeBranches + ", " + useNegativeNumbers + ");");
            generateAndRun(treeDepth, treeBranches, useNegativeNumbers);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Test helpers

function generateAndRun(treeDepth, treeBranches, useNegativeNumbers) {
    var low_i32 = -0x80000001;
    var high_i32 = 0x7fffffff;
    var ui32 = 0xffffffff;

    var f = "";
    f += echos("(function test() {", 1);
    {
        f += echos("echo(inner(" + toHex(useNegativeNumbers ? low_i32 : high_i32) + "));");
        f += echos("function inner(a) {", 1);
        {
            f += echos("a |= 0;");
            f += echos("var r = 0;");
            buildTree(treeDepth, treeBranches).traverse(function visit(node) {
                nonwideningOperation = (nonwideningOperation + 1) % NonwideningOperation.Count;
                var nodeName = varName(node.number);
                var dst =
                    nonwideningOperation === NonwideningOperation.None || nonwideningOperation === NonwideningOperation.ConvNum
                        ? nodeName
                        : tempName(node.number);
                var parentNodeName = node.parent ? varName(node.parent.number) : "a";
                if(nonwideningOperation === NonwideningOperation.Neg)
                    parentNodeName = "-" + parentNodeName;
                else if(nonwideningOperation === NonwideningOperation.ConvNum)
                    parentNodeName = "+" + parentNodeName;
                var right =
                    node.parent && nonwideningOperation !== NonwideningOperation.Neg && (node.number & 3) === 0
                        ? useNegativeNumbers ? low_i32 : ui32
                        : parentNodeName;
                f += echos("var " + dst + " = " + parentNodeName + " + " + right + " + 1;");
                if(nonwideningOperation === NonwideningOperation.Neg)
                    f += echos("var " + nodeName + " = -" + dst + ";");
                else if(nonwideningOperation === NonwideningOperation.Ld)
                    f += echos("var " + nodeName + " = " + dst + ";");
                if(node.left || node.right)
                    return;
                f += echos("r ^= " + nodeName + ";");
            });
            f += echos("return r;");
        }
        f += echos("}", -1);
    }
    f += echos("});", -1);
    f = f.substring(0, f.length - 1);
    echo(f);
    f = eval(f);
    f();
    echo();

    function buildTree(depth, branches, numberContainer, parent) {
        function TreeNode(number, parent) {
            this.number = number;
            this.parent = parent ? parent : null;
            this.left = null;
            this.right = null;
        }
        TreeNode.prototype.traverse = function (visit) {
            visit(this);
            if(this.left)
                this.left.traverse(visit);
            if(this.right)
                this.right.traverse(visit);
        };

        if(depth <= 0)
            throw new Error("Invalid tree depth");
        if(branches <= 0)
            throw new Error("Invalid number of branches");
        if((branches & branches - 1) !== 0)
            throw new Error("Number of branches must be a power of 2");
        if(!numberContainer)
            numberContainer = [0];
        else if(numberContainer[0] < 0)
            throw new Error("Invalid node number");

        var node = new TreeNode(numberContainer[0]++, parent);
        if(--depth !== 0) {
            var branch = branches !== 1;
            if(branch)
                branches >>= 1;
            node.left = buildTree(depth, branches, numberContainer, node);
            if(branch)
                node.right = buildTree(depth, branches, numberContainer, node);
        }

        return node;
    }

    function varName(number) {
        return "v" + number;
    }

    function tempName(number) {
        return "t" + number;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

function toHex(n) {
    if(typeof n !== "number")
        return n;
    return n >= 0 ? "0x" + n.toString(16) : "-0x" + (-n).toString(16);
}

function echos(s, changeIndent) {
    if(changeIndent && changeIndent < 0)
        indent += changeIndent * 4;
    if(s && s !== "") {
        var spaces = "";
        for(var i = 0; i < indent; ++i)
            spaces += " ";
        s = spaces + s + "\n";
    }
    else if(s === "")
        s = "\n";
    else
        s = "";
    if(changeIndent && changeIndent > 0)
        indent += changeIndent * 4;
    return s;
}

function echo() {
    var doEcho;
    if(this.WScript)
        doEcho = function (s) { this.WScript.Echo(s); };
    else if(this.document)
        doEcho = function (s) {
            var div = this.document.createElement("div");
            div.innerText = s;
            this.document.body.appendChild(div);
        };
    else
        doEcho = function (s) { this.print(s); };
    echo = function () {
        var s = "";
        for(var i = 0; i < arguments.length; ++i)
            s += arguments[i];
        doEcho(s);
    };
    echo.apply(this, arguments);
}
