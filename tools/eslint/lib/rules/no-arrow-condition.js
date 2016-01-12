/**
 * @fileoverview A rule to warn against using arrow functions in conditions.
 * @author Jxck <https://github.com/Jxck>
 * @copyright 2015 Luke Karrys. All rights reserved.
 * The MIT License (MIT)

 * Copyright (c) 2015 Jxck

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a node is an arrow function expression.
 * @param {ASTNode} node - node to test
 * @returns {boolean} `true` if the node is an arrow function expression.
 */
function isArrowFunction(node) {
    return node.test && node.test.type === "ArrowFunctionExpression";
}

/**
 * Checks whether or not a node is a conditional expression.
 * @param {ASTNode} node - node to test
 * @returns {boolean} `true` if the node is a conditional expression.
 */
function isConditional(node) {
    return node.body && node.body.type === "ConditionalExpression";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    /**
     * Reports if a conditional statement is an arrow function.
     * @param {ASTNode} node - A node to check and report.
     * @returns {void}
     */
    function checkCondition(node) {
        if (isArrowFunction(node)) {
            context.report(node, "Arrow function `=>` used inside {{statementType}} instead of comparison operator.", {statementType: node.type});
        }
    }

    /**
     * Reports if an arrow function contains an ambiguous conditional.
     * @param {ASTNode} node - A node to check and report.
     * @returns {void}
     */
    function checkArrowFunc(node) {
        if (isConditional(node)) {
            context.report(node, "Arrow function used ambiguously with a conditional expression.");
        }
    }

    return {
        "IfStatement": checkCondition,
        "WhileStatement": checkCondition,
        "ForStatement": checkCondition,
        "ConditionalExpression": checkCondition,
        "ArrowFunctionExpression": checkArrowFunc
    };
};

module.exports.schema = [];
