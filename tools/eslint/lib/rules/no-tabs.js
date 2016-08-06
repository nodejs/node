/**
 * @fileoverview Rule to check for tabs inside a file
 * @author Gyandeep Singh
 */

"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------
const regex = /\t/;

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "Disallow tabs in file",
            category: "Stylistic Issues",
            recommended: false
        },
        schema: []
    },

    create(context) {
        return {
            Program(node) {
                context.getSourceLines().forEach((line, index) => {
                    const match = regex.exec(line);

                    if (match) {
                        context.report(
                            node,
                            {
                                line: index + 1,
                                column: match.index + 1
                            },
                            "Unexpected tab character."
                        );
                    }
                });
            }
        };
    }
};
