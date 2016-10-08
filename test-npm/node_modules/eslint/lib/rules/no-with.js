/**
 * @fileoverview Rule to flag use of with statement
 * @author Nicholas C. Zakas
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {
        "WithStatement": function(node) {
            context.report(node, "Unexpected use of 'with' statement.");
        }
    };

};

module.exports.schema = [];
