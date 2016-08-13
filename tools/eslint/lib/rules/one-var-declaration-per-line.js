/**
 * @fileoverview Rule to check multiple var declarations per line
 * @author Alberto Rodríguez
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "require or disallow newlines around `var` declarations",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                enum: ["always", "initializations"]
            }
        ]
    },

    create: function(context) {

        const ERROR_MESSAGE = "Expected variable declaration to be on a new line.";
        const always = context.options[0] === "always";

        //--------------------------------------------------------------------------
        // Helpers
        //--------------------------------------------------------------------------


        /**
         * Determine if provided keyword is a variant of for specifiers
         * @private
         * @param {string} keyword - keyword to test
         * @returns {boolean} True if `keyword` is a variant of for specifier
         */
        function isForTypeSpecifier(keyword) {
            return keyword === "ForStatement" || keyword === "ForInStatement" || keyword === "ForOfStatement";
        }

        /**
         * Checks newlines around variable declarations.
         * @private
         * @param {ASTNode} node - `VariableDeclaration` node to test
         * @returns {void}
         */
        function checkForNewLine(node) {
            if (isForTypeSpecifier(node.parent.type)) {
                return;
            }

            const declarations = node.declarations;
            let prev;

            declarations.forEach(function(current) {
                if (prev && prev.loc.end.line === current.loc.start.line) {
                    if (always || prev.init || current.init) {
                        context.report({
                            node: node,
                            message: ERROR_MESSAGE,
                            loc: current.loc.start
                        });
                    }
                }
                prev = current;
            });
        }

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {
            VariableDeclaration: checkForNewLine
        };

    }
};
