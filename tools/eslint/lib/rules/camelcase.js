/**
 * @fileoverview Rule to flag non-camelcased identifiers
 * @author Nicholas C. Zakas
 * @copyright 2015 Dieter Oberkofler. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    // contains reported nodes to avoid reporting twice on destructuring with shorthand notation
    var reported = [];

    /**
     * Checks if a string contains an underscore and isn't all upper-case
     * @param {String} name The string to check.
     * @returns {boolean} if the string is underscored
     * @private
     */
    function isUnderscored(name) {

        // if there's an underscore, it might be A_CONSTANT, which is okay
        return name.indexOf("_") > -1 && name !== name.toUpperCase();
    }

    /**
     * Reports an AST node as a rule violation.
     * @param {ASTNode} node The node to report.
     * @returns {void}
     * @private
     */
    function report(node) {
        if (reported.indexOf(node) < 0) {
            reported.push(node);
            context.report(node, "Identifier '{{name}}' is not in camel case.", { name: node.name });
        }
    }

    var options = context.options[0] || {},
        properties = options.properties || "";

    if (properties !== "always" && properties !== "never") {
        properties = "always";
    }

    return {

        "Identifier": function(node) {

            /*
             * Leading and trailing underscores are commonly used to flag
             * private/protected identifiers, strip them
             */
            var name = node.name.replace(/^_+|_+$/g, ""),
                effectiveParent = (node.parent.type === "MemberExpression") ? node.parent.parent : node.parent;

            // MemberExpressions get special rules
            if (node.parent.type === "MemberExpression") {

                // "never" check properties
                if (properties === "never") {
                    return;
                }

                // Always report underscored object names
                if (node.parent.object.type === "Identifier" &&
                        node.parent.object.name === node.name &&
                        isUnderscored(name)) {
                    report(node);

                // Report AssignmentExpressions only if they are the left side of the assignment
                } else if (effectiveParent.type === "AssignmentExpression" &&
                        isUnderscored(name) &&
                        (effectiveParent.right.type !== "MemberExpression" ||
                        effectiveParent.left.type === "MemberExpression" &&
                        effectiveParent.left.property.name === node.name)) {
                    report(node);
                }

            // Properties have their own rules
            } else if (node.parent.type === "Property") {

                // "never" check properties
                if (properties === "never") {
                    return;
                }

                if (node.parent.parent && node.parent.parent.type === "ObjectPattern" &&
                        node.parent.key === node && node.parent.value !== node) {
                    return;
                }

                if (isUnderscored(name) && effectiveParent.type !== "CallExpression") {
                    report(node);
                }

            // Report anything that is underscored that isn't a CallExpression
            } else if (isUnderscored(name) && effectiveParent.type !== "CallExpression") {
                report(node);
            }
        }

    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "properties": {
                "enum": ["always", "never"]
            }
        },
        "additionalProperties": false
    }
];
