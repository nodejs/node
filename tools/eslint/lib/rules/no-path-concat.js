/**
 * @fileoverview Disallow string concatenation when using __dirname and __filename
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var MATCHER = /^__(?:dir|file)name$/;

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "BinaryExpression": function(node) {

            var left = node.left,
                right = node.right;

            if (node.operator === "+" &&
                    ((left.type === "Identifier" && MATCHER.test(left.name)) ||
                    (right.type === "Identifier" && MATCHER.test(right.name)))
            ) {

                context.report(node, "Use path.join() or path.resolve() instead of + to create paths.");
            }
        }

    };

};
