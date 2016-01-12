/**
 * @fileoverview Rule to flag when label is not used for a loop or switch
 * @author Ilya Volodin
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "LabeledStatement": function(node) {
            var type = node.body.type;

            if (type !== "ForStatement" && type !== "WhileStatement" && type !== "DoWhileStatement" && type !== "SwitchStatement" && type !== "ForInStatement" && type !== "ForOfStatement") {
                context.report(node, "Unexpected label \"{{l}}\"", {l: node.label.name});
            }
        }
    };

};

module.exports.schema = [];
