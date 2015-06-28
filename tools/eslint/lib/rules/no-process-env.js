/**
 * @fileoverview Disallow the use of process.env()
 * @author Vignesh Anand
 * @copyright 2014 Vignesh Anand. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    return {

        "MemberExpression": function(node) {
            var objectName = node.object.name,
                propertyName = node.property.name;

            if (objectName === "process" && !node.computed && propertyName && propertyName === "env") {
                context.report(node, "Unexpected use of process.env.");
            }

        }

    };

};

module.exports.schema = [];
