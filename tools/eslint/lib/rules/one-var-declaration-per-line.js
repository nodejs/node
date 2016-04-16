/**
 * @fileoverview Rule to check multiple var declarations per line
 * @author Alberto Rodríguez
 * @copyright 2016 Alberto Rodríguez. All rights reserved.
 * See LICENSE file in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var ERROR_MESSAGE = "Expected variable declaration to be on a new line.";
    var always = context.options[0] === "always";

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------


    /**
     * Determine if provided keyword is a variant of for specifiers
     * @private
     * @param {string} keyword - keyword to test
     * @returns {boolean} True if `keyword` is a variant of for specifier
     */
    function isForTypeSpecifier(keyword) {
        return keyword === "ForStatement" || keyword === "ForInStatement" || keyword === "ForOfStatement";
    }

    /**
     * Checks newlines around variable declarations.
     * @private
     * @param {ASTNode} node - `VariableDeclaration` node to test
     * @returns {void}
     */
    function checkForNewLine(node) {
        if (isForTypeSpecifier(node.parent.type)) {
            return;
        }

        var declarations = node.declarations;
        var prev;

        declarations.forEach(function(current) {
            if (prev && prev.loc.end.line === current.loc.start.line) {
                if (always || prev.init || current.init) {
                    context.report({
                        node: node,
                        message: ERROR_MESSAGE,
                        loc: current.loc.start
                    });
                }
            }
            prev = current;
        });
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "VariableDeclaration": checkForNewLine
    };

};

module.exports.schema = [
    {
        "enum": ["always", "initializations"]
    }
];
