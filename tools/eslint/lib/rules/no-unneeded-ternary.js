/**
 * @fileoverview Rule to flag no-unneeded-ternary
 * @author Gyandeep Singh
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 * @copyright 2015 Michael Ficarra. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Reports an AST node as a rule violation.
     * @param {ASTNode} mainNode The node to report.
     * @param {object} culpritNode - The token which has a problem
     * @returns {void}
     * @private
     */
    function report(mainNode, culpritNode) {
        context.report(mainNode, culpritNode.loc.start, "Unnecessary use of boolean literals in conditional expression");
    }

    /**
     * Test if the node is a boolean literal
     * @param {ASTNode} node - The node to report.
     * @returns {boolean} True if the its a boolean literal
     * @private
     */
    function isBooleanLiteral(node) {
        return node.type === "Literal" && typeof node.value === "boolean";
    }

    return {

        "ConditionalExpression": function(node) {

            if (isBooleanLiteral(node.alternate) && isBooleanLiteral(node.consequent)) {
                report(node, node.consequent);
            }
        }
    };
};

module.exports.schema = [];
