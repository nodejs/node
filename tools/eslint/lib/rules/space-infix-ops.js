/**
 * @fileoverview Require spaces around infix operators
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require spacing around operators",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "whitespace",

        schema: [
            {
                type: "object",
                properties: {
                    int32Hint: {
                        type: "boolean"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {
        var int32Hint = context.options[0] ? context.options[0].int32Hint === true : false;

        var OPERATORS = [
            "*", "/", "%", "+", "-", "<<", ">>", ">>>", "<", "<=", ">", ">=", "in",
            "instanceof", "==", "!=", "===", "!==", "&", "^", "|", "&&", "||", "=",
            "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", ">>>=", "&=", "^=", "|=",
            "?", ":", ",", "**"
        ];

        /**
         * Returns the first token which violates the rule
         * @param {ASTNode} left - The left node of the main node
         * @param {ASTNode} right - The right node of the main node
         * @returns {object} The violator token or null
         * @private
         */
        function getFirstNonSpacedToken(left, right) {
            var op,
                tokens = context.getTokensBetween(left, right, 1);

            for (var i = 1, l = tokens.length - 1; i < l; ++i) {
                op = tokens[i];
                if (
                    op.type === "Punctuator" &&
                    OPERATORS.indexOf(op.value) >= 0 &&
                    (tokens[i - 1].range[1] >= op.range[0] || op.range[1] >= tokens[i + 1].range[0])
                ) {
                    return op;
                }
            }
            return null;
        }

        /**
         * Reports an AST node as a rule violation
         * @param {ASTNode} mainNode - The node to report
         * @param {object} culpritToken - The token which has a problem
         * @returns {void}
         * @private
         */
        function report(mainNode, culpritToken) {
            context.report({
                node: mainNode,
                loc: culpritToken.loc.start,
                message: "Infix operators must be spaced.",
                fix: function(fixer) {
                    var previousToken = context.getTokenBefore(culpritToken);
                    var afterToken = context.getTokenAfter(culpritToken);
                    var fixString = "";

                    if (culpritToken.range[0] - previousToken.range[1] === 0) {
                        fixString = " ";
                    }

                    fixString += culpritToken.value;

                    if (afterToken.range[0] - culpritToken.range[1] === 0) {
                        fixString += " ";
                    }

                    return fixer.replaceText(culpritToken, fixString);
                }
            });
        }

        /**
         * Check if the node is binary then report
         * @param {ASTNode} node node to evaluate
         * @returns {void}
         * @private
         */
        function checkBinary(node) {
            var nonSpacedNode = getFirstNonSpacedToken(node.left, node.right);

            if (nonSpacedNode) {
                if (!(int32Hint && context.getSource(node).substr(-2) === "|0")) {
                    report(node, nonSpacedNode);
                }
            }
        }

        /**
         * Check if the node is conditional
         * @param {ASTNode} node node to evaluate
         * @returns {void}
         * @private
         */
        function checkConditional(node) {
            var nonSpacedConsequesntNode = getFirstNonSpacedToken(node.test, node.consequent);
            var nonSpacedAlternateNode = getFirstNonSpacedToken(node.consequent, node.alternate);

            if (nonSpacedConsequesntNode) {
                report(node, nonSpacedConsequesntNode);
            } else if (nonSpacedAlternateNode) {
                report(node, nonSpacedAlternateNode);
            }
        }

        /**
         * Check if the node is a variable
         * @param {ASTNode} node node to evaluate
         * @returns {void}
         * @private
         */
        function checkVar(node) {
            var nonSpacedNode;

            if (node.init) {
                nonSpacedNode = getFirstNonSpacedToken(node.id, node.init);
                if (nonSpacedNode) {
                    report(node, nonSpacedNode);
                }
            }
        }

        return {
            AssignmentExpression: checkBinary,
            AssignmentPattern: checkBinary,
            BinaryExpression: checkBinary,
            LogicalExpression: checkBinary,
            ConditionalExpression: checkConditional,
            VariableDeclarator: checkVar
        };

    }
};
