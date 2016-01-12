/**
 * @fileoverview Validates JSDoc comments are syntactically correct
 * @author Nicholas C. Zakas
 * @copyright 2014 Nicholas C. Zakas. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var doctrine = require("doctrine");

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    var options = context.options[0] || {},
        prefer = options.prefer || {},
        sourceCode = context.getSourceCode(),

        // these both default to true, so you have to explicitly make them false
        requireReturn = options.requireReturn !== false,
        requireParamDescription = options.requireParamDescription !== false,
        requireReturnDescription = options.requireReturnDescription !== false,
        requireReturnType = options.requireReturnType !== false;

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    // Using a stack to store if a function returns or not (handling nested functions)
    var fns = [];

    /**
     * Check if node type is a Class
     * @param {ASTNode} node node to check.
     * @returns {boolean} True is its a class
     * @private
     */
    function isTypeClass(node) {
        return node.type === "ClassExpression" || node.type === "ClassDeclaration";
    }

    /**
     * When parsing a new function, store it in our function stack.
     * @param {ASTNode} node A function node to check.
     * @returns {void}
     * @private
     */
    function startFunction(node) {
        fns.push({
            returnPresent: (node.type === "ArrowFunctionExpression" && node.body.type !== "BlockStatement") ||
                isTypeClass(node)
        });
    }

    /**
     * Indicate that return has been found in the current function.
     * @param {ASTNode} node The return node.
     * @returns {void}
     * @private
     */
    function addReturn(node) {
        var functionState = fns[fns.length - 1];

        if (functionState && node.argument !== null) {
            functionState.returnPresent = true;
        }
    }

    /**
     * Check if return tag type is void or undefined
     * @param {Object} tag JSDoc tag
     * @returns {boolean} True if its of type void or undefined
     * @private
     */
    function isValidReturnType(tag) {
        return tag.type === null || tag.type.name === "void" || tag.type.type === "UndefinedLiteral";
    }

    /**
     * Validate the JSDoc node and output warnings if anything is wrong.
     * @param {ASTNode} node The AST node to check.
     * @returns {void}
     * @private
     */
    function checkJSDoc(node) {
        var jsdocNode = sourceCode.getJSDocComment(node),
            functionData = fns.pop(),
            hasReturns = false,
            hasConstructor = false,
            isOverride = false,
            params = Object.create(null),
            jsdoc;

        // make sure only to validate JSDoc comments
        if (jsdocNode) {

            try {
                jsdoc = doctrine.parse(jsdocNode.value, {
                    strict: true,
                    unwrap: true,
                    sloppy: true
                });
            } catch (ex) {

                if (/braces/i.test(ex.message)) {
                    context.report(jsdocNode, "JSDoc type missing brace.");
                } else {
                    context.report(jsdocNode, "JSDoc syntax error.");
                }

                return;
            }

            jsdoc.tags.forEach(function(tag) {

                switch (tag.title) {

                    case "param":
                    case "arg":
                    case "argument":
                        if (!tag.type) {
                            context.report(jsdocNode, "Missing JSDoc parameter type for '{{name}}'.", { name: tag.name });
                        }

                        if (!tag.description && requireParamDescription) {
                            context.report(jsdocNode, "Missing JSDoc parameter description for '{{name}}'.", { name: tag.name });
                        }

                        if (params[tag.name]) {
                            context.report(jsdocNode, "Duplicate JSDoc parameter '{{name}}'.", { name: tag.name });
                        } else if (tag.name.indexOf(".") === -1) {
                            params[tag.name] = 1;
                        }
                        break;

                    case "return":
                    case "returns":
                        hasReturns = true;

                        if (!requireReturn && !functionData.returnPresent && (tag.type === null || !isValidReturnType(tag))) {
                            context.report(jsdocNode, "Unexpected @" + tag.title + " tag; function has no return statement.");
                        } else {
                            if (requireReturnType && !tag.type) {
                                context.report(jsdocNode, "Missing JSDoc return type.");
                            }

                            if (!isValidReturnType(tag) && !tag.description && requireReturnDescription) {
                                context.report(jsdocNode, "Missing JSDoc return description.");
                            }
                        }

                        break;

                    case "constructor":
                    case "class":
                        hasConstructor = true;
                        break;

                    case "override":
                    case "inheritdoc":
                        isOverride = true;
                        break;

                    // no default
                }

                // check tag preferences
                if (prefer.hasOwnProperty(tag.title) && tag.title !== prefer[tag.title]) {
                    context.report(jsdocNode, "Use @{{name}} instead.", { name: prefer[tag.title] });
                }

            });

            // check for functions missing @returns
            if (!isOverride && !hasReturns && !hasConstructor && node.parent.kind !== "get" && !isTypeClass(node)) {
                if (requireReturn || functionData.returnPresent) {
                    context.report(jsdocNode, "Missing JSDoc @" + (prefer.returns || "returns") + " for function.");
                }
            }

            // check the parameters
            var jsdocParams = Object.keys(params);

            if (node.params) {
                node.params.forEach(function(param, i) {
                    var name = param.name;

                    // TODO(nzakas): Figure out logical things to do with destructured, default, rest params
                    if (param.type === "Identifier") {
                        if (jsdocParams[i] && (name !== jsdocParams[i])) {
                            context.report(jsdocNode, "Expected JSDoc for '{{name}}' but found '{{jsdocName}}'.", {
                                name: name,
                                jsdocName: jsdocParams[i]
                            });
                        } else if (!params[name] && !isOverride) {
                            context.report(jsdocNode, "Missing JSDoc for parameter '{{name}}'.", {
                                name: name
                            });
                        }
                    }
                });
            }

            if (options.matchDescription) {
                var regex = new RegExp(options.matchDescription);

                if (!regex.test(jsdoc.description)) {
                    context.report(jsdocNode, "JSDoc description does not satisfy the regex pattern.");
                }
            }

        }

    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "ArrowFunctionExpression": startFunction,
        "FunctionExpression": startFunction,
        "FunctionDeclaration": startFunction,
        "ClassExpression": startFunction,
        "ClassDeclaration": startFunction,
        "ArrowFunctionExpression:exit": checkJSDoc,
        "FunctionExpression:exit": checkJSDoc,
        "FunctionDeclaration:exit": checkJSDoc,
        "ClassExpression:exit": checkJSDoc,
        "ClassDeclaration:exit": checkJSDoc,
        "ReturnStatement": addReturn
    };

};

module.exports.schema = [
    {
        "type": "object",
        "properties": {
            "prefer": {
                "type": "object",
                "additionalProperties": {
                    "type": "string"
                }
            },
            "requireReturn": {
                "type": "boolean"
            },
            "requireParamDescription": {
                "type": "boolean"
            },
            "requireReturnDescription": {
                "type": "boolean"
            },
            "matchDescription": {
                "type": "string"
            },
            "requireReturnType": {
                "type": "boolean"
            }
        },
        "additionalProperties": false
    }
];
