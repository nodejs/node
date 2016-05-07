/**
 * @fileoverview Specify the maximum number of statements allowed per line.
 * @author Kenneth Williams
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce a maximum number of statements allowed per line",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    max: {
                        type: "integer"
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {

        var options = context.options[0] || {},
            lastStatementLine = 0,
            numberOfStatementsOnThisLine = 0,
            maxStatementsPerLine = typeof options.max !== "undefined" ? options.max : 1;

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------

        /**
         * Reports a node
         * @param {ASTNode} node The node to report
         * @returns {void}
         * @private
         */
        function report(node) {
            context.report(
                node,
                "This line has too many statements. Maximum allowed is {{max}}.",
                { max: maxStatementsPerLine });
        }

        /**
         * Enforce a maximum number of statements per line
         * @param {ASTNode} nodes Array of nodes to evaluate
         * @returns {void}
         * @private
         */
        function enforceMaxStatementsPerLine(nodes) {
            if (nodes.length < 1) {
                return;
            }

            for (var i = 0, l = nodes.length; i < l; ++i) {
                var currentStatement = nodes[i];

                if (currentStatement.loc.start.line === lastStatementLine) {
                    ++numberOfStatementsOnThisLine;
                } else {
                    numberOfStatementsOnThisLine = 1;
                    lastStatementLine = currentStatement.loc.end.line;
                }
                if (numberOfStatementsOnThisLine === maxStatementsPerLine + 1) {
                    report(currentStatement);
                }
            }
        }

        /**
         * Check each line in the body of a node
         * @param {ASTNode} node node to evaluate
         * @returns {void}
         * @private
         */
        function checkLinesInBody(node) {
            enforceMaxStatementsPerLine(node.body);
        }

        /**
         * Check each line in the consequent of a switch case
         * @param {ASTNode} node node to evaluate
         * @returns {void}
         * @private
         */
        function checkLinesInConsequent(node) {
            enforceMaxStatementsPerLine(node.consequent);
        }

        //--------------------------------------------------------------------------
        // Public API
        //--------------------------------------------------------------------------

        return {
            Program: checkLinesInBody,
            BlockStatement: checkLinesInBody,
            SwitchCase: checkLinesInConsequent
        };

    }
};
