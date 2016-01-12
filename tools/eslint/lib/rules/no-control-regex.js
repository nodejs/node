/**
 * @fileoverview Rule to forbid control charactes from regular expressions.
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Get the regex expression
     * @param {ASTNode} node node to evaluate
     * @returns {*} Regex if found else null
     * @private
     */
    function getRegExp(node) {

        if (node.value instanceof RegExp) {
            return node.value;
        } else if (typeof node.value === "string") {

            var parent = context.getAncestors().pop();
            if ((parent.type === "NewExpression" || parent.type === "CallExpression") &&
            parent.callee.type === "Identifier" && parent.callee.name === "RegExp") {

                // there could be an invalid regular expression string
                try {
                    return new RegExp(node.value);
                } catch (ex) {
                    return null;
                }

            }
        } else {
            return null;
        }

    }



    return {

        "Literal": function(node) {

            var computedValue,
                regex = getRegExp(node);

            if (regex) {
                computedValue = regex.toString();
                if (/[\x00-\x1f]/.test(computedValue)) {
                    context.report(node, "Unexpected control character in regular expression.");
                }
            }
        }
    };

};

module.exports.schema = [];
