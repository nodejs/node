/**
 * @fileoverview Require file to end with single newline.
 * @author Nodeca Team <https://github.com/nodeca>
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "enforce at least one newline at the end of files",
            category: "Stylistic Issues",
            recommended: false
        },

        fixable: "whitespace",

        schema: [
            {
                enum: ["unix", "windows"]
            }
        ]
    },

    create: function(context) {

        //--------------------------------------------------------------------------
        // Public
        //--------------------------------------------------------------------------

        return {

            Program: function checkBadEOF(node) {

                // Get the whole source code, not for node only.
                var src = context.getSource(),
                    location = {column: 1},
                    linebreakStyle = context.options[0] || "unix",
                    linebreak = linebreakStyle === "unix" ? "\n" : "\r\n";

                if (src[src.length - 1] !== "\n") {

                    // file is not newline-terminated
                    location.line = src.split(/\n/g).length;
                    context.report({
                        node: node,
                        loc: location,
                        message: "Newline required at end of file but not found.",
                        fix: function(fixer) {
                            return fixer.insertTextAfterRange([0, src.length], linebreak);
                        }
                    });
                }
            }

        };

    }
};
