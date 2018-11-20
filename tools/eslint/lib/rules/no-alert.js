/**
 * @fileoverview Rule to flag use of alert, confirm, prompt
 * @author Nicholas C. Zakas
 */
"use strict";

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

function matchProhibited(name) {
    return name.match(/^(alert|confirm|prompt)$/);
}

function report(context, node, result) {
    context.report(node, "Unexpected {{name}}.", { name: result[1] });
}


//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "CallExpression": function(node) {

            var result;

            // without window.
            if (node.callee.type === "Identifier") {

                result = matchProhibited(node.callee.name);

                if (result) {
                    report(context, node, result);
                }

            } else if (node.callee.type === "MemberExpression" && node.callee.property.type === "Identifier") {

                result = matchProhibited(node.callee.property.name);

                if (result && node.callee.object.name === "window") {
                    report(context, node, result);
                }

            }

        }
    };

};
