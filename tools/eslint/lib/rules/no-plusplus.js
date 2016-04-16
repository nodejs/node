/**
 * @fileoverview Rule to flag use of unary increment and decrement operators.
 * @author Ian Christian Myers
 * @author Brody McKee (github.com/mrmckeb)
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var config = context.options[0],
        allowInForAfterthought = false;

    if (typeof config === "object") {
        allowInForAfterthought = config.allowForLoopAfterthoughts === true;
    }

    return {

        "UpdateExpression": function(node) {
            if (allowInForAfterthought && node.parent.type === "ForStatement") {
                return;
            }
            context.report(node, "Unary operator '" + node.operator + "' used.");
        }

    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "allowForLoopAfterthoughts": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
