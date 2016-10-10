/**
 * @fileoverview Validates newlines before and after dots
 * @author Greg Cochard
 * @copyright 2015 Greg Cochard
 */

"use strict";

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var config = context.options[0],
        // default to onObject if no preference is passed
        onObject = config === "object" || !config;

    /**
     * Reports if the dot between object and property is on the correct loccation.
     * @param {ASTNode} obj The object owning the property.
     * @param {ASTNode} prop The property of the object.
     * @param {ASTNode} node The corresponding node of the token.
     * @returns {void}
     */
    function checkDotLocation(obj, prop, node) {
        var dot = context.getTokenBefore(prop);

        if (dot.type === "Punctuator" && dot.value === ".") {
            if (onObject) {
                if (!astUtils.isTokenOnSameLine(obj, dot)) {
                    context.report(node, dot.loc.start, "Expected dot to be on same line as object.");
                }
            } else if (!astUtils.isTokenOnSameLine(dot, prop)) {
                context.report(node, dot.loc.start, "Expected dot to be on same line as property.");
            }
        }
    }

    /**
     * Checks the spacing of the dot within a member expression.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     */
    function checkNode(node) {
        checkDotLocation(node.object, node.property, node);
    }

    return {
        "MemberExpression": checkNode
    };
};

module.exports.schema = [
    {
        "enum": ["object", "property"]
    }
];
