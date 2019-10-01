/**
 * @fileoverview Rule to flag comparisons to the value NaN
 * @author James Allardice
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

/**
 * Determines if the given node is a NaN `Identifier` node.
 * @param {ASTNode|null} node The node to check.
 * @returns {boolean} `true` if the node is 'NaN' identifier.
 */
function isNaNIdentifier(node) {
    return Boolean(node) && node.type === "Identifier" && node.name === "NaN";
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "require calls to `isNaN()` when checking for `NaN`",
            category: "Possible Errors",
            recommended: true,
            url: "https://eslint.org/docs/rules/use-isnan"
        },

        schema: [
            {
                type: "object",
                properties: {
                    enforceForSwitchCase: {
                        type: "boolean",
                        default: false
                    }
                },
                additionalProperties: false
            }
        ],

        messages: {
            comparisonWithNaN: "Use the isNaN function to compare with NaN.",
            switchNaN: "'switch(NaN)' can never match a case clause. Use Number.isNaN instead of the switch.",
            caseNaN: "'case NaN' can never match. Use Number.isNaN before the switch."
        }
    },

    create(context) {

        const enforceForSwitchCase = context.options[0] && context.options[0].enforceForSwitchCase;

        /**
         * Checks the given `BinaryExpression` node.
         * @param {ASTNode} node The node to check.
         * @returns {void}
         */
        function checkBinaryExpression(node) {
            if (
                /^(?:[<>]|[!=]=)=?$/u.test(node.operator) &&
                (isNaNIdentifier(node.left) || isNaNIdentifier(node.right))
            ) {
                context.report({ node, messageId: "comparisonWithNaN" });
            }
        }

        /**
         * Checks the discriminant and all case clauses of the given `SwitchStatement` node.
         * @param {ASTNode} node The node to check.
         * @returns {void}
         */
        function checkSwitchStatement(node) {
            if (isNaNIdentifier(node.discriminant)) {
                context.report({ node, messageId: "switchNaN" });
            }

            for (const switchCase of node.cases) {
                if (isNaNIdentifier(switchCase.test)) {
                    context.report({ node: switchCase, messageId: "caseNaN" });
                }
            }
        }

        const listeners = {
            BinaryExpression: checkBinaryExpression
        };

        if (enforceForSwitchCase) {
            listeners.SwitchStatement = checkSwitchStatement;
        }

        return listeners;
    }
};
