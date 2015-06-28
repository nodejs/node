/**
 * @fileoverview Rule to flag trailing underscores in variable declarations.
 * @author Matt DuVall <http://www.mattduvall.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    //-------------------------------------------------------------------------
    // Helpers
    //-------------------------------------------------------------------------

    function hasTrailingUnderscore(identifier) {
        var len = identifier.length;

        return identifier !== "_" && (identifier[0] === "_" || identifier[len - 1] === "_");
    }

    function isSpecialCaseIdentifierForMemberExpression(identifier) {
        return identifier === "__proto__";
    }

    function isSpecialCaseIdentifierInVariableExpression(identifier) {
        // Checks for the underscore library usage here
        return identifier === "_";
    }

    function checkForTrailingUnderscoreInFunctionDeclaration(node) {
        if (node.id) {
            var identifier = node.id.name;

            if (typeof identifier !== "undefined" && hasTrailingUnderscore(identifier)) {
                context.report(node, "Unexpected dangling \"_\" in \"" + identifier + "\".");
            }
        }
    }

    function checkForTrailingUnderscoreInVariableExpression(node) {
        var identifier = node.id.name;

        if (typeof identifier !== "undefined" && hasTrailingUnderscore(identifier) &&
            !isSpecialCaseIdentifierInVariableExpression(identifier)) {
            context.report(node, "Unexpected dangling \"_\" in \"" + identifier + "\".");
        }
    }

    function checkForTrailingUnderscoreInMemberExpression(node) {
        var identifier = node.property.name;

        if (typeof identifier !== "undefined" && hasTrailingUnderscore(identifier) &&
            !isSpecialCaseIdentifierForMemberExpression(identifier)) {
            context.report(node, "Unexpected dangling \"_\" in \"" + identifier + "\".");
        }
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {
        "FunctionDeclaration": checkForTrailingUnderscoreInFunctionDeclaration,
        "VariableDeclarator": checkForTrailingUnderscoreInVariableExpression,
        "MemberExpression": checkForTrailingUnderscoreInMemberExpression
    };

};

module.exports.schema = [];
