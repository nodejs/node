/**
 * @fileoverview Rule to ensure newline per method call when chaining calls
 * @author Rajendra Patil
 * @copyright 2016 Rajendra Patil. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var options = context.options[0] || {},
        codeStateMap = {},
        ignoreChainWithDepth = options.ignoreChainWithDepth || 2;

    /**
     * Check and Capture State if the chained calls/members
     * @param {ASTNode} node The node to check
     * @param {object} codeState The state of the code current code to be filled
     * @returns {void}
     */
    function checkAndCaptureStateRecursively(node, codeState) {
        var valid = false,
            objectLineNumber,
            propertyLineNumber;
        if (node.callee) {
            node = node.callee;
            codeState.hasFunctionCall = true;
        }

        if (node.object) {
            codeState.depth++;

            objectLineNumber = node.object.loc.end.line;
            propertyLineNumber = node.property.loc.end.line;
            valid = node.computed || propertyLineNumber > objectLineNumber;

            if (!valid) {
                codeState.reports.push({
                    node: node,
                    text: "Expected line break after `{{code}}`.",
                    depth: codeState.depth
                });
            }
            // Recurse
            checkAndCaptureStateRecursively(node.object, codeState);
        }

    }
    /**
     * Verify and report the captured state report
     * @param {object} codeState contains the captured state with `hasFunctionCall, reports and depth`
     * @returns {void}
     */
    function reportState(codeState) {
        var report;
        if (codeState.hasFunctionCall && codeState.depth > ignoreChainWithDepth && codeState.reports) {
            while (codeState.reports.length) {
                report = codeState.reports.shift();
                context.report(report.node, report.text, {
                    code: context.getSourceCode().getText(report.node.object).replace(/\r\n|\r|\n/g, "\\n") // Not to print newlines in error report
                });
            }
        }
    }

    /**
     * Initialize the node state object with default values.
     * @returns {void}
     */
    function initializeState() {
        return {
            visited: false,
            hasFunctionCall: false,
            depth: 1,
            reports: []
        };
    }
    /**
     * Process the said node and recurse internally
     * @param {ASTNode} node The node to check
     * @returns {void}
     */
    function processNode(node) {
        var stateKey = [node.loc.start.line, node.loc.start.column].join("@"),
            codeState = codeStateMap[stateKey] = (codeStateMap[stateKey] || initializeState());
        if (!codeState.visited) {
            codeState.visited = true;
            checkAndCaptureStateRecursively(node, codeState);
        }
        reportState(codeState);
    }

    return {
        "CallExpression": processNode,
        "MemberExpression": processNode
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
