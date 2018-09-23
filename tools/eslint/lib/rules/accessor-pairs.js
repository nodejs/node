/**
 * @fileoverview Rule to flag wrapping non-iife in parens
 * @author Gyandeep Singh
 * @copyright 2015 Gyandeep Singh. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var config = context.options[0] || {};
    var checkGetWithoutSet = config.getWithoutSet === true;
    var checkSetWithoutGet = config.setWithoutGet !== false;

    /**
     * Checks a object expression to see if it has setter and getter both present or none.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function checkLonelySetGet(node) {
        var isSetPresent = false;
        var isGetPresent = false;
        var propLength = node.properties.length;

        for (var i = 0; i < propLength; i++) {
            var propToCheck = node.properties[i].kind === "init" ? node.properties[i].key.name : node.properties[i].kind;

            switch (propToCheck) {
                case "set":
                    isSetPresent = true;
                    break;

                case "get":
                    isGetPresent = true;
                    break;

                default:
                    // Do nothing
            }

            if (isSetPresent && isGetPresent) {
                break;
            }
        }

        if (checkSetWithoutGet && isSetPresent && !isGetPresent) {
            context.report(node, "Getter is not present");
        } else if (checkGetWithoutSet && isGetPresent && !isSetPresent) {
            context.report(node, "Setter is not present");
        }
    }

    return {
        "ObjectExpression": function (node) {
            if (checkSetWithoutGet || checkGetWithoutSet) {
                checkLonelySetGet(node);
            }
        }
    };

};
