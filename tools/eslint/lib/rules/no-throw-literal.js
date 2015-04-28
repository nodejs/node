/**
 * @fileoverview Rule to restrict what can be thrown as an exception.
 * @author Dieter Oberkofler
 * @copyright 2015 Dieter Oberkofler. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "ThrowStatement": function(node) {

            if (node.argument.type === "Literal") {
                context.report(node, "Do not throw a literal.");
            } else if (node.argument.type === "Identifier") {
                if (node.argument.name === "undefined") {
                    context.report(node, "Do not throw undefined.");
                }
            }

        }

    };

};
