/**
 * @fileoverview Wrapper around estraverse
 * @author Nicholas C. Zakas
 * @copyright 2016 Nicholas C. Zakas. All rights reserved.
 * See LICENSE in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var estraverse = require("estraverse");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

var KEY_BLACKLIST = [
    "parent",
    "leadingComments",
    "trailingComments"
];

/**
 * Wrapper around an estraverse controller that ensures the correct keys
 * are visited.
 * @constructor
 */
function Traverser() {

    var controller = Object.create(new estraverse.Controller()),
        originalTraverse = controller.traverse;

    // intercept call to traverse() and add the fallback key to the visitor
    controller.traverse = function(node, visitor) {
        visitor.fallback = Traverser.getKeys;
        return originalTraverse.call(this, node, visitor);
    };

    return controller;
}

/**
 * Calculates the keys to use for traversal.
 * @param {ASTNode} node The node to read keys from.
 * @returns {string[]} An array of keys to visit on the node.
 * @private
 */
Traverser.getKeys = function(node) {
    return Object.keys(node).filter(function(key) {
        return KEY_BLACKLIST.indexOf(key) === -1;
    });
};

module.exports = Traverser;


