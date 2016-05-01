/**
 * @fileoverview Rule to check for jsdoc presence.
 * @author Gyandeep Singh
 */
"use strict";

var lodash = require("lodash");

module.exports = {
    meta: {
        docs: {
            description: "require JSDoc comments",
            category: "Stylistic Issues",
            recommended: false
        },

        schema: [
            {
                type: "object",
                properties: {
                    require: {
                        type: "object",
                        properties: {
                            ClassDeclaration: {
                                type: "boolean"
                            },
                            MethodDefinition: {
                                type: "boolean"
                            },
                            FunctionDeclaration: {
                                type: "boolean"
                            }
                        },
                        additionalProperties: false
                    }
                },
                additionalProperties: false
            }
        ]
    },

    create: function(context) {
        var source = context.getSourceCode();
        var DEFAULT_OPTIONS = {
            FunctionDeclaration: true,
            MethodDefinition: false,
            ClassDeclaration: false
        };
        var options = lodash.assign(DEFAULT_OPTIONS, context.options[0] && context.options[0].require || {});

        /**
         * Report the error message
         * @param {ASTNode} node node to report
         * @returns {void}
         */
        function report(node) {
            context.report(node, "Missing JSDoc comment.");
        }

        /**
         * Check if the jsdoc comment is present for class methods
         * @param {ASTNode} node node to examine
         * @returns {void}
         */
        function checkClassMethodJsDoc(node) {
            if (node.parent.type === "MethodDefinition") {
                var jsdocComment = source.getJSDocComment(node);

                if (!jsdocComment) {
                    report(node);
                }
            }
        }

        /**
         * Check if the jsdoc comment is present or not.
         * @param {ASTNode} node node to examine
         * @returns {void}
         */
        function checkJsDoc(node) {
            var jsdocComment = source.getJSDocComment(node);

            if (!jsdocComment) {
                report(node);
            }
        }

        return {
            FunctionDeclaration: function(node) {
                if (options.FunctionDeclaration) {
                    checkJsDoc(node);
                }
            },
            FunctionExpression: function(node) {
                if (options.MethodDefinition) {
                    checkClassMethodJsDoc(node);
                }
            },
            ClassDeclaration: function(node) {
                if (options.ClassDeclaration) {
                    checkJsDoc(node);
                }
            }
        };
    }
};
