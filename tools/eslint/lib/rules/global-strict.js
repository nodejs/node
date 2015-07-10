/**
 * @fileoverview Rule to flag or require global strict mode.
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var mode = context.options[0];

    if (mode === "always") {

        return {
            "Program": function(node) {
                if (node.body.length > 0) {
                    var statement = node.body[0];

                    if (!(statement.type === "ExpressionStatement" && statement.expression.value === "use strict")) {
                        context.report(node, "Use the global form of \"use strict\".");
                    }
                }
            }
        };

    } else { // mode = "never"

        return {
            "ExpressionStatement": function(node) {
                var parent = context.getAncestors().pop();

                if (node.expression.value === "use strict" && parent.type === "Program") {
                    context.report(node, "Use the function form of \"use strict\".");
                }
            }
        };

    }

};

module.exports.schema = [
    {
        "enum": ["always", "never"]
    }
];
