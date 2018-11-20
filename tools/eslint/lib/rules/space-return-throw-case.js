/**
 * @fileoverview Require spaces following return, throw, and case
 * @author Michael Ficarra
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    function check(node) {
        var tokens = context.getFirstTokens(node, 2),
            value = tokens[0].value;

        if (tokens[0].range[1] >= tokens[1].range[0]) {
            context.report(node, "Keyword \"" + value + "\" must be followed by whitespace.");
        }
    }

    return {
        "ReturnStatement": function(node) {
            if (node.argument) {
                check(node);
            }
        },
        "SwitchCase": function(node) {
            if (node.test) {
                check(node);
            }
        },
        "ThrowStatement": check
    };

};
