/**
 * @fileoverview Rule to flag statements that use magic numbers (adapted from https://github.com/danielstjules/buddy.js)
 * @author Vincent Lemeunier
 * @copyright 2015 Vincent Lemeunier. All rights reserved.
 *
 * This rule was adapted from danielstjules/buddy.js
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Daniel St. Jules
 *
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
 *
 * See LICENSE file in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var config = context.options[0] || {},
        detectObjects = !!config.detectObjects,
        enforceConst = !!config.enforceConst,
        ignore = config.ignore || [],
        ignoreArrayIndexes = !!config.ignoreArrayIndexes;

    /**
     * Returns whether the node is number literal
     * @param {Node} node - the node literal being evaluated
     * @returns {boolean} true if the node is a number literal
     */
    function isNumber(node) {
        return typeof node.value === "number";
    }

    /**
     * Returns whether the number should be ignored
     * @param {number} num - the number
     * @returns {boolean} true if the number should be ignored
     */
    function shouldIgnoreNumber(num) {
        return ignore.indexOf(num) !== -1;
    }

    /**
     * Returns whether the number should be ignored when used as a radix within parseInt() or Number.parseInt()
     * @param {ASTNode} parent - the non-"UnaryExpression" parent
     * @param {ASTNode} node - the node literal being evaluated
     * @returns {boolean} true if the number should be ignored
     */
    function shouldIgnoreParseInt(parent, node) {
        return parent.type === "CallExpression" && node === parent.arguments[1] &&
            (parent.callee.name === "parseInt" ||
            parent.callee.type === "MemberExpression" &&
            parent.callee.object.name === "Number" &&
            parent.callee.property.name === "parseInt");
    }

    /**
     * Returns whether the number should be ignored when used to define a JSX prop
     * @param {ASTNode} parent - the non-"UnaryExpression" parent
     * @returns {boolean} true if the number should be ignored
     */
    function shouldIgnoreJSXNumbers(parent) {
        return parent.type.indexOf("JSX") === 0;
    }

    /**
     * Returns whether the number should be ignored when used as an array index with enabled 'ignoreArrayIndexes' option.
     * @param {ASTNode} parent - the non-"UnaryExpression" parent.
     * @returns {boolean} true if the number should be ignored
     */
    function shouldIgnoreArrayIndexes(parent) {
        return parent.type === "MemberExpression" && ignoreArrayIndexes;
    }

    return {
        "Literal": function(node) {
            var parent = node.parent,
                value = node.value,
                raw = node.raw,
                okTypes = detectObjects ? [] : ["ObjectExpression", "Property", "AssignmentExpression"];

            if (!isNumber(node)) {
                return;
            }

            // For negative magic numbers: update the value and parent node
            if (parent.type === "UnaryExpression" && parent.operator === "-") {
                node = parent;
                parent = node.parent;
                value = -value;
                raw = "-" + raw;
            }

            if (shouldIgnoreNumber(value) ||
                shouldIgnoreParseInt(parent, node) ||
                shouldIgnoreArrayIndexes(parent) ||
                shouldIgnoreJSXNumbers(parent)) {
                return;
            }

            if (parent.type === "VariableDeclarator") {
                if (enforceConst && parent.parent.kind !== "const") {
                    context.report({
                        node: node,
                        message: "Number constants declarations must use 'const'"
                    });
                }
            } else if (okTypes.indexOf(parent.type) === -1) {
                context.report({
                    node: node,
                    message: "No magic number: " + raw
                });
            }
        }
    };
};

module.exports.schema = [{
    "type": "object",
    "properties": {
        "detectObjects": {
            "type": "boolean"
        },
        "enforceConst": {
            "type": "boolean"
        },
        "ignore": {
            "type": "array",
            "items": {
                "type": "number"
            },
            "uniqueItems": true
        },
        "ignoreArrayIndexes": {
            "type": "boolean"
        }
    },
    "additionalProperties": false
}];
