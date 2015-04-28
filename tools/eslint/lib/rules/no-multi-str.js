/**
 * @fileoverview Rule to flag when using multiline strings
 * @author Ilya Volodin
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Determines if a given node is part of JSX syntax.
     * @param {ASTNode} node The node to check.
     * @returns {boolean} True if the node is a JSX node, false if not.
     * @private
     */
    function isJSXElement(node) {
        return node.type.indexOf("JSX") === 0;
    }

    //--------------------------------------------------------------------------
    // Public API
    //--------------------------------------------------------------------------

    return {

        "Literal": function(node) {
            var lineBreak = /\n/;

            if (lineBreak.test(node.raw) && !isJSXElement(node.parent)) {
                context.report(node, "Multiline support is limited to browsers supporting ES5 only.");
            }
        }
    };

};
