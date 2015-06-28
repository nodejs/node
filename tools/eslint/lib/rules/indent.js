/**
 * @fileoverview This option sets a specific tab width for your code
 * This rule has been ported and modified from JSCS.
 * @author Dmitriy Shekhovtsov
 * @copyright 2015 Dmitriy Shekhovtsov. All rights reserved.
 * @copyright 2013 Dulin Marat and other contributors.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*eslint no-use-before-define:[2, "nofunc"]*/
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function (context) {
    // indentation defaults: 4 spaces
    var indentChar = " ";
    var indentSize = 4;
    var options = {indentSwitchCase: false};

    var lines = null;
    var indentStack = [0];
    var linesToCheck = null;
    var breakIndents = null;

    if (context.options.length) {
        if (context.options[0] === "tab") {
            indentChar = "\t";
            indentSize = 1;
        } else /* istanbul ignore else : this will be caught by options validation */ if (typeof context.options[0] === "number") {
            indentSize = context.options[0];
        }

        if (context.options[1]) {
            var opts = context.options[1];
            options.indentSwitchCase = opts.indentSwitchCase === true;
        }
    }

    var blockParents = [
        "IfStatement",
        "WhileStatement",
        "DoWhileStatement",
        "ForStatement",
        "ForInStatement",
        "ForOfStatement",
        "FunctionDeclaration",
        "FunctionExpression",
        "ArrowExpression",
        "CatchClause",
        "WithStatement"
    ];

    var indentableNodes = {
        BlockStatement: "body",
        Program: "body",
        ObjectExpression: "properties",
        ArrayExpression: "elements",
        SwitchStatement: "cases"
    };

    if (options.indentSwitchCase) {
        indentableNodes.SwitchCase = "consequent";
    }

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Mark line to be checked
     * @param {Number} line - line number
     * @returns {void}
     */
    function markCheckLine(line) {
        linesToCheck[line].check = true;
    }

    /**
     * Mark line with targeted node to be checked
     * @param {ASTNode} checkNode - targeted node
     * @returns {void}
     */
    function markCheck(checkNode) {
        markCheckLine(checkNode.loc.start.line - 1);
    }

    /**
     * Sets pushing indent of current node
     * @param {ASTNode} node - targeted node
     * @param {Number} indents - indents count to push
     * @returns {void}
     */
    function markPush(node, indents) {
        linesToCheck[node.loc.start.line - 1].push.push(indents);
    }

    /**
     * Marks line as outdent, end of block statement for example
     * @param {ASTNode} node - targeted node
     * @param {Number} outdents - count of outedents in targeted line
     * @returns {void}
     */
    function markPop(node, outdents) {
        linesToCheck[node.loc.end.line - 1].pop.push(outdents);
    }

    /**
     * Set alt push for current node
     * @param {ASTNode} node - targeted node
     * @returns {void}
     */
    function markPushAlt(node) {
        linesToCheck[node.loc.start.line - 1].pushAltLine.push(node.loc.end.line - 1);
    }

    /**
     * Marks end of node block to be checked
     * and marks targeted node as indent pushing
     * @param {ASTNode} pushNode - targeted node
     * @param {Number} indents - indent count to push
     * @returns {void}
     */
    function markPushAndEndCheck(pushNode, indents) {
        markPush(pushNode, indents);
        markCheckLine(pushNode.loc.end.line - 1);
    }

    /**
     * Mark node as switch case statement
     * and set push\pop indentation changes
     * @param {ASTNode} caseNode - targeted node
     * @param {ASTNode[]} children - consequent child nodes of case node
     * @returns {void}
     */
    function markCase(caseNode, children) {
        var outdentNode = getCaseOutdent(children);

        if (outdentNode) {
            // If a case statement has a `break` as a direct child and it is the
            // first one encountered, use it as the example for all future case indentation
            if (breakIndents === null) {
                breakIndents = (caseNode.loc.start.column === outdentNode.loc.start.column) ? 1 : 0;
            }
            markPop(outdentNode, breakIndents);
        } else {
            markPop(caseNode, 0);
        }
    }

    /**
     * Mark child nodes to be checked later of targeted node,
     * only if child node not in same line as targeted one
     * (if child and parent nodes wrote in single line)
     * @param {ASTNode} node - targeted node
     * @returns {void}
     */
    function markChildren(node) {
        getChildren(node).forEach(function(childNode) {
            if (childNode.loc.start.line !== node.loc.start.line || node.type === "Program") {
                markCheck(childNode);
            }
        });
    }

    /**
     * Mark child block as scope pushing and mark to check
     * @param {ASTNode} node - target node
     * @param {String} property - target node property containing child
     * @returns {void}
     */
    function markAlternateBlockStatement(node, property) {
        var child = node[property];
        if (child && child.type === "BlockStatement") {
            markCheck(child);
        }
    }

    /**
     * Checks whether node is multiline or single line
     * @param {ASTNode} node - target node
     * @returns {boolean} - is multiline node
     */
    function isMultiline(node) {
        return node.loc.start.line !== node.loc.end.line;
    }

    /**
     * Get switch case statement outdent node if any
     * @param {ASTNode[]} caseChildren - case statement childs
     * @returns {ASTNode} - outdent node
     */
    function getCaseOutdent(caseChildren) {
        var outdentNode;
        caseChildren.some(function(node) {
            if (node.type === "BreakStatement") {
                outdentNode = node;
                return true;
            }
        });

        return outdentNode;
    }

    /**
     * Returns block containing node
     * @param {ASTNode} node - targeted node
     * @returns {ASTNode} - block node
     */
    function getBlockNodeToMark(node) {
        var parent = node.parent;

        // The parent of an else is the entire if/else block. To avoid over indenting
        // in the case of a non-block if with a block else, mark push where the else starts,
        // not where the if starts!
        if (parent.type === "IfStatement" && parent.alternate === node) {
            return node;
        }

        // The end line to check of a do while statement needs to be the location of the
        // closing curly brace, not the while statement, to avoid marking the last line of
        // a multiline while as a line to check.
        if (parent.type === "DoWhileStatement") {
            return node;
        }

        // Detect bare blocks: a block whose parent doesn"t expect blocks in its syntax specifically.
        if (blockParents.indexOf(parent.type) === -1) {
            return node;
        }

        return parent;
    }

    /**
     * Get node's children
     * @param {ASTNode} node - current node
     * @returns {ASTNode[]} - children
     */
    function getChildren(node) {
        var childrenProperty = indentableNodes[node.type];
        return node[childrenProperty];
    }

    /**
     * Gets indentation in line `i`
     * @param {Number} i - number of line to get indentation
     * @returns {Number} - count of indentation symbols
     */
    function getIndentationFromLine(i) {
        var rNotIndentChar = new RegExp("[^" + indentChar + "]");
        var firstContent = lines[i].search(rNotIndentChar);
        if (firstContent === -1) {
            firstContent = lines[i].length;
        }
        return firstContent;
    }

    /**
     * Compares expected and actual indentation
     * and reports any violations
     * @param {ASTNode} node - node used only for reporting
     * @returns {void}
     */
    function checkIndentations(node) {
        linesToCheck.forEach(function(line, i) {
            var actualIndentation = getIndentationFromLine(i);
            var expectedIndentation = getExpectedIndentation(line, actualIndentation);

            if (line.check) {

                if (actualIndentation !== expectedIndentation) {
                    context.report(node,
                        {line: i + 1, column: expectedIndentation},
                        "Expected indentation of " + expectedIndentation + " characters.");
                    // correct the indentation so that future lines
                    // can be validated appropriately
                    actualIndentation = expectedIndentation;
                }
            }

            if (line.push.length) {
                pushExpectedIndentations(line, actualIndentation);
            }
        });
    }

    /**
     * Counts expected indentation for given line number
     * @param {Number} line - line number
     * @param {Number} actual - actual indentation
     * @returns {number} - expected indentation
     */
    function getExpectedIndentation(line, actual) {
        var outdent = indentSize * Math.max.apply(null, line.pop);

        var idx = indentStack.length - 1;
        var expected = indentStack[idx];

        if (!Array.isArray(expected)) {
            expected = [expected];
        }

        expected = expected.map(function(value) {
            if (line.pop.length) {
                value -= outdent;
            }

            return value;
        }).reduce(function(previous, current) {
            // when the expected is an array, resolve the value
            // back into a Number by checking both values are the actual indentation
            return actual === current ? current : previous;
        });

        indentStack[idx] = expected;

        line.pop.forEach(function() {
            indentStack.pop();
        });

        return expected;
    }

    /**
     * Store in stack expected indentations
     * @param {Number} line - current line
     * @param {Number} actualIndentation - actual indentation at current line
     * @returns {void}
     */
    function pushExpectedIndentations(line, actualIndentation) {
        var indents = Math.max.apply(null, line.push);
        var expected = actualIndentation + (indentSize * indents);

        // when a line has alternate indentations, push an array of possible values
        // on the stack, to be resolved when checked against an actual indentation
        if (line.pushAltLine.length) {
            expected = [expected];
            line.pushAltLine.forEach(function(altLine) {
                expected.push(getIndentationFromLine(altLine) + (indentSize * indents));
            });
        }

        line.push.forEach(function() {
            indentStack.push(expected);
        });
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "Program": function (node) {
            lines = context.getSourceLines();
            linesToCheck = lines.map(function () {
                return {
                    push: [],
                    pushAltLine: [],
                    pop: [],
                    check: false
                };
            });

            if (!isMultiline(node)) {
                return;
            }

            markChildren(node);
        },
        "Program:exit": function (node) {
            checkIndentations(node);
        },

        "BlockStatement": function (node) {
            if (!isMultiline(node)) {
                return;
            }

            markChildren(node);
            markPop(node, 1);

            markPushAndEndCheck(getBlockNodeToMark(node), 1);
        },

        "IfStatement": function (node) {
            markAlternateBlockStatement(node, "alternate");
        },

        "TryStatement": function (node) {
            markAlternateBlockStatement(node, "handler");
            markAlternateBlockStatement(node, "finalizer");
        },

        "SwitchStatement": function (node) {
            if (!isMultiline(node)) {
                return;
            }

            var indents = 1;
            var children = getChildren(node);

            if (children.length && node.loc.start.column === children[0].loc.start.column) {
                indents = 0;
            }

            markChildren(node);
            markPop(node, indents);
            markPushAndEndCheck(node, indents);
        },

        "SwitchCase": function (node) {
            if (!options.indentSwitchCase) {
                return;
            }

            if (!isMultiline(node)) {
                return;
            }

            var children = getChildren(node);

            if (children.length === 1 && children[0].type === "BlockStatement") {
                return;
            }

            markPush(node, 1);
            markCheck(node);
            markChildren(node);

            markCase(node, children);
        },

        // indentations inside of function expressions can be offset from
        // either the start of the function or the end of the function, therefore
        // mark all starting lines of functions as potential indentations
        "FunctionDeclaration": function (node) {
            markPushAlt(node);
        },
        "FunctionExpression": function (node) {
            markPushAlt(node);
        }
    };

};

module.exports.schema = [
    {
        "oneOf": [
            {
                "enum": ["tab"]
            },
            {
                "type": "integer"
            }
        ]
    },
    {
        "type": "object",
        "properties": {
            "indentSwitchCase": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
