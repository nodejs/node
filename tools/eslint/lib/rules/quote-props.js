/**
 * @fileoverview Rule to flag non-quoted property names in object literals.
 * @author Mathias Bynens <http://mathiasbynens.be/>
 * @copyright 2014 Brandon Mills. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var espree = require("espree");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var MODE = context.options[0];

    /**
     * Ensures that a property's key is quoted only when necessary
     * @param   {ASTNode} node Property AST node
     * @returns {void}
     */
    function asNeeded(node) {
        var key = node.key,
            tokens;

        if (key.type === "Literal" && typeof key.value === "string") {
            try {
                tokens = espree.tokenize(key.value);
            } catch (e) {
                return;
            }

            if (tokens.length === 1 &&
                (["Identifier", "Null", "Boolean"].indexOf(tokens[0].type) >= 0 ||
                (tokens[0].type === "Numeric" && "" + +tokens[0].value === tokens[0].value))
            ) {
                context.report(node, "Unnecessarily quoted property `{{value}}` found.", key);
            }
        }
    }

    /**
     * Ensures that a property's key is quoted
     * @param   {ASTNode} node Property AST node
     * @returns {void}
     */
    function always(node) {
        var key = node.key;

        if (!node.method && !(key.type === "Literal" && typeof key.value === "string")) {
            context.report(node, "Unquoted property `{{key}}` found.", {
                key: key.name || key.value
            });
        }
    }

    return {
        "Property": MODE === "as-needed" ? asNeeded : always
    };

};
