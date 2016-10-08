/**
 * @fileoverview Rule to flag when initializing to undefined
 * @author Ilya Volodin
 * @copyright 2013 Ilya Volodin. All rights reserved.
 * See LICENSE in root directory for full license.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "VariableDeclarator": function(node) {
            var name = node.id.name,
                init = node.init && node.init.name;

            if (init === "undefined" && node.parent.kind !== "const") {
                context.report(node, "It's not necessary to initialize '{{name}}' to undefined.", { name: name });
            }
        }
    };

};

module.exports.schema = [];
