/**
 * @fileoverview Rule to check that spaced function application
 * @author Matt DuVall <http://www.mattduvall.com>
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    function detectOpenSpaces(node) {
        var lastCalleeToken = context.getLastToken(node.callee);
        var tokens = context.getTokens(node);
        var i = tokens.indexOf(lastCalleeToken), l = tokens.length;
        while (i < l && tokens[i].value !== "(") {
            ++i;
        }
        if (i >= l) {
            return;
        }
        // look for a space between the callee and the open paren
        if (tokens[i - 1].range[1] !== tokens[i].range[0]) {
            context.report(node, "Unexpected space between function name and paren.");
        }
    }

    return {
        "CallExpression": detectOpenSpaces,
        "NewExpression": detectOpenSpaces
    };

};

module.exports.schema = [];
