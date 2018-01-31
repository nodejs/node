/**
 * @fileoverview Rule to flag use of continue statement
 * @author Borislav Zhivkov
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow `continue` statements",
            category: "Stylistic Issues",
            recommended: false,
            url: "https://eslint.org/docs/rules/no-continue"
        },

        schema: []
    },

    create(context) {

        return {
            ContinueStatement(node) {
                context.report({ node, message: "Unexpected use of continue statement." });
            }
        };

    }
};
