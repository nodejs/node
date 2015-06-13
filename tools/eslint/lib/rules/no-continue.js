/**
 * @fileoverview Rule to flag use of continue statement
 * @author Borislav Zhivkov
 * @copyright 2015 Borislav Zhivkov. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "ContinueStatement": function(node) {
            context.report(node, "Unexpected use of continue statement");
        }
    };

};
