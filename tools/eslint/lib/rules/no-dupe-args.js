/**
 * @fileoverview Rule to flag duplicate arguments
 * @author Jamund Ferguson
 * @copyright 2015 Jamund Ferguson. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    /**
     * Determines if a given node has duplicate parameters.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function checkParams(node) {
        var params = {},
            dups = {};


        /**
         * Marks a given param as either seen or duplicated.
         * @param {string} name The name of the param to mark.
         * @returns {void}
         * @private
         */
        function markParam(name) {
            if (params.hasOwnProperty(name)) {
                dups[name] = 1;
            } else {
                params[name] = 1;
            }
        }

        // loop through and find each duplicate param
        node.params.forEach(function(param) {

            switch (param.type) {
                case "Identifier":
                    markParam(param.name);
                    break;

                case "ObjectPattern":
                    param.properties.forEach(function(property) {
                        markParam(property.key.name);
                    });
                    break;

                case "ArrayPattern":
                    param.elements.forEach(function(element) {

                        // Arrays can be sparse (unwanted arguments)
                        if (element !== null) {
                            markParam(element.name);
                        }
                    });
                    break;

                // no default
            }
        });

        // log an error for each duplicate (not 2 for each)
        Object.keys(dups).forEach(function(currentParam) {
            context.report(node, "Duplicate param '{{key}}'.", { key: currentParam });
        });
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "FunctionDeclaration": checkParams,
        "FunctionExpression": checkParams
    };

};

module.exports.schema = [];
