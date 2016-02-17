/**
 * @fileoverview Disallow parenthesising higher precedence subexpressions.
 * @author Michael Ficarra
 * @copyright 2014 Michael Ficarra. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var ALL_NODES = context.options[0] !== "functions";

    /**
     * Determines if this rule should be enforced for a node given the current configuration.
     * @param {ASTNode} node - The node to be checked.
     * @returns {boolean} True if the rule should be enforced for this node.
     * @private
     */
    function ruleApplies(node) {
        return ALL_NODES || node.type === "FunctionExpression" || node.type === "ArrowFunctionExpression";
    }

    /**
     * Determines if a node is surrounded by parentheses.
     * @param {ASTNode} node - The node to be checked.
     * @returns {boolean} True if the node is parenthesised.
     * @private
     */
    function isParenthesised(node) {
        var previousToken = context.getTokenBefore(node),
            nextToken = context.getTokenAfter(node);

        return previousToken && nextToken &&
            previousToken.value === "(" && previousToken.range[1] <= node.range[0] &&
            nextToken.value === ")" && nextToken.range[0] >= node.range[1];
    }

    /**
     * Determines if a node is surrounded by parentheses twice.
     * @param {ASTNode} node - The node to be checked.
     * @returns {boolean} True if the node is doubly parenthesised.
     * @private
     */
    function isParenthesisedTwice(node) {
        var previousToken = context.getTokenBefore(node, 1),
            nextToken = context.getTokenAfter(node, 1);

        return isParenthesised(node) && previousToken && nextToken &&
            previousToken.value === "(" && previousToken.range[1] <= node.range[0] &&
            nextToken.value === ")" && nextToken.range[0] >= node.range[1];
    }

    /**
     * Determines if a node is surrounded by (potentially) invalid parentheses.
     * @param {ASTNode} node - The node to be checked.
     * @returns {boolean} True if the node is incorrectly parenthesised.
     * @private
     */
    function hasExcessParens(node) {
        return ruleApplies(node) && isParenthesised(node);
    }

    /**
     * Determines if a node that is expected to be parenthesised is surrounded by
     * (potentially) invalid extra parentheses.
     * @param {ASTNode} node - The node to be checked.
     * @returns {boolean} True if the node is has an unexpected extra pair of parentheses.
     * @private
     */
    function hasDoubleExcessParens(node) {
        return ruleApplies(node) && isParenthesisedTwice(node);
    }

    /**
     * Checks whether or not a given node is located at the head of ExpressionStatement.
     * @param {ASTNode} node - A node to check.
     * @returns {boolean} `true` if the node is located at the head of ExpressionStatement.
     */
    function isHeadOfExpressionStatement(node) {
        var parent = node.parent;
        while (parent) {
            switch (parent.type) {
                case "SequenceExpression":
                    if (parent.expressions[0] !== node || isParenthesised(node)) {
                        return false;
                    }
                    break;

                case "UnaryExpression":
                case "UpdateExpression":
                    if (parent.prefix || isParenthesised(node)) {
                        return false;
                    }
                    break;

                case "BinaryExpression":
                case "LogicalExpression":
                    if (parent.left !== node || isParenthesised(node)) {
                        return false;
                    }
                    break;

                case "ConditionalExpression":
                    if (parent.test !== node || isParenthesised(node)) {
                        return false;
                    }
                    break;

                case "CallExpression":
                    if (parent.callee !== node || isParenthesised(node)) {
                        return false;
                    }
                    break;

                case "MemberExpression":
                    if (parent.object !== node || isParenthesised(node)) {
                        return false;
                    }
                    break;

                case "ExpressionStatement":
                    return true;

                default:
                    return false;
            }

            node = parent;
            parent = parent.parent;
        }

        /* istanbul ignore next */
        throw new Error("unreachable");
    }

    /**
     * Get the precedence level based on the node type
     * @param {ASTNode} node node to evaluate
     * @returns {int} precedence level
     * @private
     */
    function precedence(node) {

        switch (node.type) {
            case "SequenceExpression":
                return 0;

            case "AssignmentExpression":
            case "ArrowFunctionExpression":
            case "YieldExpression":
                return 1;

            case "ConditionalExpression":
                return 3;

            case "LogicalExpression":
                switch (node.operator) {
                    case "||":
                        return 4;
                    case "&&":
                        return 5;
                    // no default
                }

                /* falls through */
            case "BinaryExpression":
                switch (node.operator) {
                    case "|":
                        return 6;
                    case "^":
                        return 7;
                    case "&":
                        return 8;
                    case "==":
                    case "!=":
                    case "===":
                    case "!==":
                        return 9;
                    case "<":
                    case "<=":
                    case ">":
                    case ">=":
                    case "in":
                    case "instanceof":
                        return 10;
                    case "<<":
                    case ">>":
                    case ">>>":
                        return 11;
                    case "+":
                    case "-":
                        return 12;
                    case "*":
                    case "/":
                    case "%":
                        return 13;
                    // no default
                }
                /* falls through */
            case "UnaryExpression":
                return 14;
            case "UpdateExpression":
                return 15;
            case "CallExpression":
                // IIFE is allowed to have parens in any position (#655)
                if (node.callee.type === "FunctionExpression") {
                    return -1;
                }
                return 16;
            case "NewExpression":
                return 17;
            // no default
        }
        return 18;
    }

    /**
     * Report the node
     * @param {ASTNode} node node to evaluate
     * @returns {void}
     * @private
     */
    function report(node) {
        var previousToken = context.getTokenBefore(node);
        context.report(node, previousToken.loc.start, "Gratuitous parentheses around expression.");
    }

    /**
     * Evaluate Unary update
     * @param {ASTNode} node node to evaluate
     * @returns {void}
     * @private
     */
    function dryUnaryUpdate(node) {
        if (hasExcessParens(node.argument) && precedence(node.argument) >= precedence(node)) {
            report(node.argument);
        }
    }

    /**
     * Evaluate a new call
     * @param {ASTNode} node node to evaluate
     * @returns {void}
     * @private
     */
    function dryCallNew(node) {
        if (hasExcessParens(node.callee) && precedence(node.callee) >= precedence(node) && !(
            node.type === "CallExpression" &&
            node.callee.type === "FunctionExpression" &&
            // One set of parentheses are allowed for a function expression
            !hasDoubleExcessParens(node.callee)
        )) {
            report(node.callee);
        }
        if (node.arguments.length === 1) {
            if (hasDoubleExcessParens(node.arguments[0]) && precedence(node.arguments[0]) >= precedence({type: "AssignmentExpression"})) {
                report(node.arguments[0]);
            }
        } else {
            [].forEach.call(node.arguments, function(arg) {
                if (hasExcessParens(arg) && precedence(arg) >= precedence({type: "AssignmentExpression"})) {
                    report(arg);
                }
            });
        }
    }

    /**
     * Evaluate binary logicals
     * @param {ASTNode} node node to evaluate
     * @returns {void}
     * @private
     */
    function dryBinaryLogical(node) {
        var prec = precedence(node);
        if (hasExcessParens(node.left) && precedence(node.left) >= prec) {
            report(node.left);
        }
        if (hasExcessParens(node.right) && precedence(node.right) > prec) {
            report(node.right);
        }
    }

    return {
        "ArrayExpression": function(node) {
            [].forEach.call(node.elements, function(e) {
                if (e && hasExcessParens(e) && precedence(e) >= precedence({type: "AssignmentExpression"})) {
                    report(e);
                }
            });
        },
        "ArrowFunctionExpression": function(node) {
            if (node.body.type !== "BlockStatement") {
                if (node.body.type !== "ObjectExpression" && hasExcessParens(node.body) && precedence(node.body) >= precedence({type: "AssignmentExpression"})) {
                    report(node.body);
                    return;
                }

                // Object literals *must* be parenthesized
                if (node.body.type === "ObjectExpression" && hasDoubleExcessParens(node.body)) {
                    report(node.body);
                    return;
                }
            }
        },
        "AssignmentExpression": function(node) {
            if (hasExcessParens(node.right) && precedence(node.right) >= precedence(node)) {
                report(node.right);
            }
        },
        "BinaryExpression": dryBinaryLogical,
        "CallExpression": dryCallNew,
        "ConditionalExpression": function(node) {
            if (hasExcessParens(node.test) && precedence(node.test) >= precedence({type: "LogicalExpression", operator: "||"})) {
                report(node.test);
            }
            if (hasExcessParens(node.consequent) && precedence(node.consequent) >= precedence({type: "AssignmentExpression"})) {
                report(node.consequent);
            }
            if (hasExcessParens(node.alternate) && precedence(node.alternate) >= precedence({type: "AssignmentExpression"})) {
                report(node.alternate);
            }
        },
        "DoWhileStatement": function(node) {
            if (hasDoubleExcessParens(node.test)) {
                report(node.test);
            }
        },
        "ExpressionStatement": function(node) {
            var firstToken;
            if (hasExcessParens(node.expression)) {
                firstToken = context.getFirstToken(node.expression);

                // Pure object literals ({}) do not need parentheses but
                // member expressions do ({}.toString())
                if ((
                        firstToken.value !== "{" ||
                        node.expression.type === "ObjectExpression"
                    ) &&
                    // For such as `(function(){}.foo.bar)`
                    (
                        firstToken.value !== "function" ||
                        node.expression.type === "FunctionExpression"
                    ) &&
                    // For such as `(class{}.foo.bar)`
                    (
                        firstToken.value !== "class" ||
                        node.expression.type === "ClassExpression"
                    )
                ) {
                    report(node.expression);
                }
            }
        },
        "ForInStatement": function(node) {
            if (hasExcessParens(node.right)) {
                report(node.right);
            }
        },
        "ForOfStatement": function(node) {
            if (hasExcessParens(node.right)) {
                report(node.right);
            }
        },
        "ForStatement": function(node) {
            if (node.init && hasExcessParens(node.init)) {
                report(node.init);
            }

            if (node.test && hasExcessParens(node.test)) {
                report(node.test);
            }

            if (node.update && hasExcessParens(node.update)) {
                report(node.update);
            }
        },
        "IfStatement": function(node) {
            if (hasDoubleExcessParens(node.test)) {
                report(node.test);
            }
        },
        "LogicalExpression": dryBinaryLogical,
        "MemberExpression": function(node) {
            if (
                hasExcessParens(node.object) &&
                precedence(node.object) >= precedence(node) &&
                (
                    node.computed ||
                    !(
                        (node.object.type === "Literal" &&
                        typeof node.object.value === "number" &&
                        /^[0-9]+$/.test(context.getFirstToken(node.object).value))
                        ||
                        // RegExp literal is allowed to have parens (#1589)
                        (node.object.type === "Literal" && node.object.regex)
                    )
                ) &&
                !(
                    (node.object.type === "FunctionExpression" || node.object.type === "ClassExpression") &&
                    isHeadOfExpressionStatement(node) &&
                    !hasDoubleExcessParens(node.object)
                )
            ) {
                report(node.object);
            }
            if (node.computed && hasExcessParens(node.property)) {
                report(node.property);
            }
        },
        "NewExpression": dryCallNew,
        "ObjectExpression": function(node) {
            [].forEach.call(node.properties, function(e) {
                var v = e.value;
                if (v && hasExcessParens(v) && precedence(v) >= precedence({type: "AssignmentExpression"})) {
                    report(v);
                }
            });
        },
        "ReturnStatement": function(node) {
            if (node.argument && hasExcessParens(node.argument) &&
                    // RegExp literal is allowed to have parens (#1589)
                    !(node.argument.type === "Literal" && node.argument.regex)) {
                report(node.argument);
            }
        },
        "SequenceExpression": function(node) {
            [].forEach.call(node.expressions, function(e) {
                if (hasExcessParens(e) && precedence(e) >= precedence(node)) {
                    report(e);
                }
            });
        },
        "SwitchCase": function(node) {
            if (node.test && hasExcessParens(node.test)) {
                report(node.test);
            }
        },
        "SwitchStatement": function(node) {
            if (hasDoubleExcessParens(node.discriminant)) {
                report(node.discriminant);
            }
        },
        "ThrowStatement": function(node) {
            if (hasExcessParens(node.argument)) {
                report(node.argument);
            }
        },
        "UnaryExpression": dryUnaryUpdate,
        "UpdateExpression": dryUnaryUpdate,
        "VariableDeclarator": function(node) {
            if (node.init && hasExcessParens(node.init) &&
                    precedence(node.init) >= precedence({type: "AssignmentExpression"}) &&
                    // RegExp literal is allowed to have parens (#1589)
                    !(node.init.type === "Literal" && node.init.regex)) {
                report(node.init);
            }
        },
        "WhileStatement": function(node) {
            if (hasDoubleExcessParens(node.test)) {
                report(node.test);
            }
        },
        "WithStatement": function(node) {
            if (hasDoubleExcessParens(node.object)) {
                report(node.object);
            }
        }
    };

};

module.exports.schema = [
    {
        "enum": ["all", "functions"]
    }
];
