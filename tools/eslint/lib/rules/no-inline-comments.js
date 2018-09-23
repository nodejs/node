/**
 * @fileoverview Enforces or disallows inline comments.
 * @author Greg Cochard
 * @copyright 2014 Greg Cochard. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Will check that comments are not on lines starting with or ending with code
     * @param {ASTNode} node The comment node to check
     * @private
     * @returns {void}
     */
    function testCodeAroundComment(node) {

        // Get the whole line and cut it off at the start of the comment
        var startLine = String(context.getSourceLines()[node.loc.start.line - 1]);
        var endLine = String(context.getSourceLines()[node.loc.end.line - 1]);

        var preamble = startLine.slice(0, node.loc.start.column).trim();

        // Also check after the comment
        var postamble = endLine.slice(node.loc.end.column).trim();

        // Should be empty if there was only whitespace around the comment
        if (preamble || postamble) {
            context.report(node, "Unexpected comment inline with code.");
        }
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {

        "LineComment": testCodeAroundComment,
        "BlockComment": testCodeAroundComment

    };
};

module.exports.schema = [];
