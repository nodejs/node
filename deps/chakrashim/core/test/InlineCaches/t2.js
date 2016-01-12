//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

SplayTreeNode = function () {
};

function insertA(node) {
    node.left = root;
    node.right = root.right;
    root.right = null;
    root = node;
};

function insertB(node) {
    node.right = root;
    node.left = root.left;
    root.left = null;
    root = node;
};

function remove() {
    var right = root.right;
    root = root.left;
    root.right = right;
};

SplayTreeNode.prototype.left = null;
SplayTreeNode.prototype.right = null;

var a = new SplayTreeNode();
var b = new SplayTreeNode();
var c = new SplayTreeNode();
var d = new SplayTreeNode();

var root = new SplayTreeNode();

insertA(a);
insertB(b);
insertA(c);
remove();
insertA(d);
remove();

// expected: [object Object] [object Object] [object Object]
WScript.Echo(root, root.left, root.right); 
