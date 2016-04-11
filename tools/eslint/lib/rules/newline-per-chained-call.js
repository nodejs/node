/**
 * @fileoverview Rule to ensure newline per method call when chaining calls
 * @author Rajendra Patil
 * @author Burak Yigit Kaya
 * @copyright 2016 Rajendra Patil. All rights reserved.
 * @copyright 2016 Burak Yigit Kaya. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var options = context.options[0] || {},
        ignoreChainWithDepth = options.ignoreChainWithDepth || 2;

    return {
        "CallExpression:exit": function(node) {
            if (!node.callee || node.callee.type !== "MemberExpression") {
                return;
            }

            var callee = node.callee;
            var parent = callee.object;
            var depth = 1;

            while (parent && parent.callee) {
                depth += 1;
                parent = parent.callee.object;
            }

            if (depth > ignoreChainWithDepth && callee.property.loc.start.line === callee.object.loc.end.line) {
                context.report(
                    callee.property,
                    callee.property.loc.start,
                    "Expected line break after `" + context.getSource(callee.object).replace(/\r\n|\r|\n/g, "\\n") + "`."
                );
            }
        }
    };
};

module.exports.schema = [{
    "type": "object",
    "properties": {
        "ignoreChainWithDepth": {
            "type": "integer",
            "minimum": 1,
            "maximum": 10
        }
    },
    "additionalProperties": false
}];
