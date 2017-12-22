/**
 * @fileoverview Rule to flag use of a debugger statement
 * @author Nicholas C. Zakas
 */

"use strict";

const astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow the use of `debugger`",
            category: "Possible Errors",
            recommended: true
        },
        fixable: "code",
        schema: []
    },

    create(context) {

        return {
            DebuggerStatement(node) {
                context.report({
                    node,
                    message: "Unexpected 'debugger' statement.",
                    fix(fixer) {
                        if (astUtils.STATEMENT_LIST_PARENTS.has(node.parent.type)) {
                            return fixer.remove(node);
                        }
                        return null;
                    }
                });
            }
        };

    }
};
