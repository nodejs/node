/**
 * @fileoverview Rule to disallow a duplicate case label.
 * @author Dieter Oberkofler
 * @copyright 2015 Dieter Oberkofler. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Get a hash value for the node
     * @param {ASTNode} node The node.
     * @returns {string} A hash value for the node.
     * @private
     */
    function getHash(node) {
        if (node.type === "Literal") {
            return node.type + typeof node.value + node.value;
        } else if (node.type === "Identifier") {
            return node.type + typeof node.name + node.name;
        } else if (node.type === "MemberExpression") {
            return node.type + getHash(node.object) + getHash(node.property);
        }
    }

    var switchStatement = [];

    return {

        "SwitchStatement": function(/*node*/) {
            switchStatement.push({});
        },

        "SwitchStatement:exit": function(/*node*/) {
            switchStatement.pop();
        },

        "SwitchCase": function(node) {
            var currentSwitch = switchStatement[switchStatement.length - 1],
                hashValue;

            if (node.test) {
                hashValue = getHash(node.test);
                if (typeof hashValue !== "undefined" && currentSwitch.hasOwnProperty(hashValue)) {
                    context.report(node, "Duplicate case label.");
                } else {
                    currentSwitch[hashValue] = true;
                }
            }
        }

    };

};
