/**
 * @fileoverview Require spaces around infix operators
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var int32Hint = context.options[0] ? context.options[0].int32Hint === true : false;

    var OPERATORS = [
        "*", "/", "%", "+", "-", "<<", ">>", ">>>", "<", "<=", ">", ">=", "in",
        "instanceof", "==", "!=", "===", "!==", "&", "^", "|", "&&", "||", "=",
        "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", ">>>=", "&=", "^=", "|=",
        "?", ":", ","
    ];

    /**
     * Returns the first token which violates the rule
     * @param {ASTNode} left - The left node of the main node
     * @param {ASTNode} right - The right node of the main node
     * @returns {object} The violator token or null
     * @private
     */
    function getFirstNonSpacedToken(left, right) {
        var op, tokens = context.getTokensBetween(left, right, 1);
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
        context.report(mainNode, culpritToken.loc.start, "Infix operators must be spaced.");
    }

    function checkBinary(node) {
        var nonSpacedNode = getFirstNonSpacedToken(node.left, node.right);

        if (nonSpacedNode) {
            if (!(int32Hint && context.getSource(node).substr(-2) === "|0")) {
                report(node, nonSpacedNode);
            }
        }
    }

    function checkConditional(node) {
        var nonSpacedConsequesntNode = getFirstNonSpacedToken(node.test, node.consequent);
        var nonSpacedAlternateNode = getFirstNonSpacedToken(node.consequent, node.alternate);

        if (nonSpacedConsequesntNode) {
            report(node, nonSpacedConsequesntNode);
        } else if (nonSpacedAlternateNode) {
            report(node, nonSpacedAlternateNode);
        }
    }

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
        "AssignmentExpression": checkBinary,
        "BinaryExpression": checkBinary,
        "LogicalExpression": checkBinary,
        "ConditionalExpression": checkConditional,
        "VariableDeclarator": checkVar
    };

};
