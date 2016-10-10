/**
 * @fileoverview Validate strings passed to the RegExp constructor
 * @author Michael Ficarra
 * @copyright 2014 Michael Ficarra. All rights reserved.
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

    /**
     * Check if node is a string
     * @param {ASTNode} node node to evaluate
     * @returns {boolean} True if its a string
     * @private
     */
    function isString(node) {
        return node && node.type === "Literal" && typeof node.value === "string";
    }

    /**
     * Validate strings passed to the RegExp constructor
     * @param {ASTNode} node node to evaluate
     * @returns {void}
     * @private
     */
    function check(node) {
        if (node.callee.type === "Identifier" && node.callee.name === "RegExp" && isString(node.arguments[0])) {
            var flags = isString(node.arguments[1]) ? node.arguments[1].value : "";

            try {
                void new RegExp(node.arguments[0].value);
            } catch (e) {
                context.report(node, e.message);
            }

            if (flags) {

                try {
                    espree.parse("/./" + flags, context.parserOptions);
                } catch (ex) {
                    context.report(node, "Invalid flags supplied to RegExp constructor '" + flags + "'");
                }
            }

        }
    }

    return {
        "CallExpression": check,
        "NewExpression": check
    };

};

module.exports.schema = [];
