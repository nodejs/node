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

        // these both default to true, so you have to explicitly make them false
        requireReturn = options.requireReturn === false ? false : true,
        requireParamDescription = options.requireParamDescription !== false,
        requireReturnDescription = options.requireReturnDescription !== false;

    //--------------------------------------------------------------------------
    // Helpers
    //--------------------------------------------------------------------------

    // Using a stack to store if a function returns or not (handling nested functions)
    var fns = [];

    /**
     * When parsing a new function, store it in our function stack.
     * @returns {void}
     * @private
     */
    function startFunction() {
        fns.push({returnPresent: false});
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
     * Validate the JSDoc node and output warnings if anything is wrong.
     * @param {ASTNode} node The AST node to check.
     * @returns {void}
     * @private
     */
    function checkJSDoc(node) {
        var jsdocNode = context.getJSDocComment(node),
            functionData = fns.pop(),
            hasReturns = false,
            hasConstructor = false,
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

                        if (!requireReturn && !functionData.returnPresent && tag.type.name !== "void" && tag.type.name !== "undefined") {
                            context.report(jsdocNode, "Unexpected @" + tag.title + " tag; function has no return statement.");
                        } else {
                            if (!tag.type) {
                                context.report(jsdocNode, "Missing JSDoc return type.");
                            }

                            if (tag.type.name !== "void" && !tag.description && requireReturnDescription) {
                                context.report(jsdocNode, "Missing JSDoc return description.");
                            }
                        }

                        break;

                    case "constructor":
                    case "class":
                        hasConstructor = true;
                        break;

                    // no default
                }

                // check tag preferences
                if (prefer.hasOwnProperty(tag.title)) {
                    context.report(jsdocNode, "Use @{{name}} instead.", { name: prefer[tag.title] });
                }

            });

            // check for functions missing @returns
            if (!hasReturns && !hasConstructor) {
                if (requireReturn || functionData.returnPresent) {
                    context.report(jsdocNode, "Missing JSDoc @returns for function.");
                }
            }

            // check the parameters
            var jsdocParams = Object.keys(params);

            node.params.forEach(function(param, i) {
                var name = param.name;

                // TODO(nzakas): Figure out logical things to do with destructured, default, rest params
                if (param.type === "Identifier") {
                    if (jsdocParams[i] && (name !== jsdocParams[i])) {
                        context.report(jsdocNode, "Expected JSDoc for '{{name}}' but found '{{jsdocName}}'.", {
                            name: name,
                            jsdocName: jsdocParams[i]
                        });
                    } else if (!params[name]) {
                        context.report(jsdocNode, "Missing JSDoc for parameter '{{name}}'.", {
                            name: name
                        });
                    }
                }
            });

        }

    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
        "ArrowFunctionExpression": startFunction,
        "FunctionExpression": startFunction,
        "FunctionDeclaration": startFunction,
        "ArrowFunctionExpression:exit": checkJSDoc,
        "FunctionExpression:exit": checkJSDoc,
        "FunctionDeclaration:exit": checkJSDoc,
        "ReturnStatement": addReturn
    };

};
