/**
 * @fileoverview A rule to warn against using arrow functions when they could be
 * confused with comparisions
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

var astUtils = require("../ast-utils.js");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Checks whether or not a node is a conditional expression.
 * @param {ASTNode} node - node to test
 * @returns {boolean} `true` if the node is a conditional expression.
 */
function isConditional(node) {
    return node && node.type === "ConditionalExpression";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var config = context.options[0] || {};

    /**
     * Reports if an arrow function contains an ambiguous conditional.
     * @param {ASTNode} node - A node to check and report.
     * @returns {void}
     */
    function checkArrowFunc(node) {
        var body = node.body;

        if (isConditional(body) && !(config.allowParens && astUtils.isParenthesised(context, body))) {
            context.report(node, "Arrow function used ambiguously with a conditional expression.");
        }
    }

    return {
        "ArrowFunctionExpression": checkArrowFunc
    };
};

module.exports.schema = [{
    type: "object",
    properties: {
        allowParens: {type: "boolean"}
    },
    additionalProperties: false
}];
