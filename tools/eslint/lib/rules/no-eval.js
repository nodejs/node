/**
 * @fileoverview Rule to flag use of eval() statement
 * @author Nicholas C. Zakas
 * @copyright 2015 Mathias Schreck. All rights reserved.
 * @copyright 2013 Nicholas C. Zakas. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "CallExpression": function(node) {
            if (node.callee.name === "eval") {
                context.report(node, "eval can be harmful.");
            }
        }
    };

};
