/**
 * @fileoverview Rule to disallow a duplicate case label.
 * @author Dieter Oberkofler
 * @author Burak Yigit Kaya
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        docs: {
            description: "disallow duplicate case labels",
            category: "Possible Errors",
            recommended: true
        },

        schema: []
    },

    create: function(context) {
        let sourceCode = context.getSourceCode();

        return {
            SwitchStatement: function(node) {
                let mapping = {};

                node.cases.forEach(function(switchCase) {
                    let key = sourceCode.getText(switchCase.test);

                    if (mapping[key]) {
                        context.report(switchCase, "Duplicate case label.");
                    } else {
                        mapping[key] = switchCase;
                    }
                });
            }
        };
    }
};
