/**
 * @fileoverview Rule to flag use of an object property of the global object (Math and JSON) as a function
 * @author James Allardice
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const { CALL, ReferenceTracker } = require("eslint-utils");

//------------------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------------------

const nonCallableGlobals = ["Atomics", "JSON", "Math", "Reflect"];

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
    meta: {
        type: "problem",

        docs: {
            description: "disallow calling global object properties as functions",
            category: "Possible Errors",
            recommended: true,
            url: "https://eslint.org/docs/rules/no-obj-calls"
        },

        schema: [],

        messages: {
            unexpectedCall: "'{{name}}' is not a function."
        }
    },

    create(context) {

        return {
            Program() {
                const scope = context.getScope();
                const tracker = new ReferenceTracker(scope);
                const traceMap = {};

                for (const global of nonCallableGlobals) {
                    traceMap[global] = {
                        [CALL]: true
                    };
                }

                for (const { node } of tracker.iterateGlobalReferences(traceMap)) {
                    context.report({ node, messageId: "unexpectedCall", data: { name: node.callee.name } });
                }
            }
        };
    }
};
