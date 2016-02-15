/**
 * @fileoverview Disallow Labeled Statements
 * @author Nicholas C. Zakas
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 * See LICENSE in root directory for full license.
 */
"use strict";

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var LOOP_TYPES = /^(?:While|DoWhile|For|ForIn|ForOf)Statement$/;

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
    var options = context.options[0];
    var allowLoop = Boolean(options && options.allowLoop);
    var allowSwitch = Boolean(options && options.allowSwitch);
    var scopeInfo = null;

    /**
     * Gets the kind of a given node.
     *
     * @param {ASTNode} node - A node to get.
     * @returns {string} The kind of the node.
     */
    function getBodyKind(node) {
        var type = node.type;

        if (LOOP_TYPES.test(type)) {
            return "loop";
        }
        if (type === "SwitchStatement") {
            return "switch";
        }
        return "other";
    }

    /**
     * Checks whether the label of a given kind is allowed or not.
     *
     * @param {string} kind - A kind to check.
     * @returns {boolean} `true` if the kind is allowed.
     */
    function isAllowed(kind) {
        switch (kind) {
            case "loop": return allowLoop;
            case "switch": return allowSwitch;
            default: return false;
        }
    }

    /**
     * Checks whether a given name is a label of a loop or not.
     *
     * @param {string} label - A name of a label to check.
     * @returns {boolean} `true` if the name is a label of a loop.
     */
    function getKind(label) {
        var info = scopeInfo;
        while (info) {
            if (info.label === label) {
                return info.kind;
            }
            info = info.upper;
        }

        /* istanbul ignore next: syntax error */
        return "other";
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "LabeledStatement": function(node) {
            scopeInfo = {
                label: node.label.name,
                kind: getBodyKind(node.body),
                upper: scopeInfo
            };
        },

        "LabeledStatement:exit": function(node) {
            if (!isAllowed(scopeInfo.kind)) {
                context.report({
                    node: node,
                    message: "Unexpected labeled statement."
                });
            }

            scopeInfo = scopeInfo.upper;
        },

        "BreakStatement": function(node) {
            if (node.label && !isAllowed(getKind(node.label.name))) {
                context.report({
                    node: node,
                    message: "Unexpected label in break statement."
                });
            }
        },

        "ContinueStatement": function(node) {
            if (node.label && !isAllowed(getKind(node.label.name))) {
                context.report({
                    node: node,
                    message: "Unexpected label in continue statement."
                });
            }
        }
    };

};

module.exports.schema = [
    {
        type: "object",
        properties: {
            allowLoop: {
                type: "boolean"
            },
            allowSwitch: {
                type: "boolean"
            }
        },
        additionalProperties: false
    }
];
