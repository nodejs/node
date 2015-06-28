/**
 * @fileoverview Checks for unreachable code due to return, throws, break, and continue.
 * @author Joel Feenstra
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------


function report(context, node, unreachableType) {
    var keyword;
    switch (unreachableType) {
        case "BreakStatement":
            keyword = "break";
            break;
        case "ContinueStatement":
            keyword = "continue";
            break;
        case "ReturnStatement":
            keyword = "return";
            break;
        case "ThrowStatement":
            keyword = "throw";
            break;
        default:
            return;
    }
    context.report(node, "Found unexpected statement after a {{type}}.", { type: keyword });
}


//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Checks if a node is an exception for no-unreachable because of variable/function hoisting
     * @param {ASTNode} node The AST node to check.
     * @returns {boolean} if the node doesn't trigger unreachable
     * @private
     */
    function isUnreachableAllowed(node) {
        return node.type === "FunctionDeclaration" ||
            node.type === "VariableDeclaration" &&
            node.declarations.every(function(declaration) {
                return declaration.type === "VariableDeclarator" && declaration.init === null;
            });
    }

    /*
     * Verifies that the given node is the last node or followed exclusively by
     * hoisted declarations
     * @param {ASTNode} node Node that should be the last node
     * @returns {void}
     * @private
     */
    function checkNode(node) {
        var parent = context.getAncestors().pop();
        var field, i, sibling;

        switch (parent.type) {
            case "SwitchCase":
                field = "consequent";
                break;
            case "Program":
            case "BlockStatement":
                field = "body";
                break;
            default:
                return;
        }

        for (i = parent[field].length - 1; i >= 0; i--) {
            sibling = parent[field][i];
            if (sibling === node) {
                return; // Found the last reachable statement, all done
            }

            if (!isUnreachableAllowed(sibling)) {
                report(context, sibling, node.type);
            }
        }
    }

    return {
        "ReturnStatement": checkNode,
        "ThrowStatement": checkNode,
        "ContinueStatement": checkNode,
        "BreakStatement": checkNode
    };

};

module.exports.schema = [];
