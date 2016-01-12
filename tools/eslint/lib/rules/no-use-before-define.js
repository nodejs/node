/**
 * @fileoverview Rule to flag use of variables before they are defined
 * @author Ilya Volodin
 * @copyright 2013 Ilya Volodin. All rights reserved.
 */

"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

var astUtils = require("../ast-utils");

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

var NO_FUNC = "nofunc";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    /**
     * Finds and validates all variables in a given scope.
     * @param {Scope} scope The scope object.
     * @returns {void}
     * @private
     */
    function findVariablesInScope(scope) {
        var typeOption = context.options[0];

        /**
         * Report the node
         * @param {object} reference reference object
         * @param {ASTNode} declaration node to evaluate
         * @returns {void}
         * @private
         */
        function checkLocationAndReport(reference, declaration) {
            if (typeOption !== NO_FUNC || declaration.defs[0].type !== "FunctionName") {
                if (declaration.identifiers[0].range[1] > reference.identifier.range[1]) {
                    context.report(reference.identifier, "\"{{a}}\" was used before it was defined", {a: reference.identifier.name});
                }
            }
        }

        scope.references.forEach(function(reference) {
            // if the reference is resolved check for declaration location
            // if not, it could be function invocation, try to find manually
            if (reference.resolved && reference.resolved.identifiers.length > 0) {
                checkLocationAndReport(reference, reference.resolved);
            } else {
                var declaration = astUtils.getVariableByName(scope, reference.identifier.name);
                // if there're no identifiers, this is a global environment variable
                if (declaration && declaration.identifiers.length !== 0) {
                    checkLocationAndReport(reference, declaration);
                }
            }
        });
    }


    /**
     * Validates variables inside of a node's scope.
     * @param {ASTNode} node The node to check.
     * @returns {void}
     * @private
     */
    function findVariables() {
        var scope = context.getScope();
        findVariablesInScope(scope);
    }

    var ruleDefinition = {
        "Program": function() {
            var scope = context.getScope();
            findVariablesInScope(scope);

            // both Node.js and Modules have an extra scope
            if (context.ecmaFeatures.globalReturn || context.ecmaFeatures.modules) {
                findVariablesInScope(scope.childScopes[0]);
            }
        }
    };

    if (context.ecmaFeatures.blockBindings) {
        ruleDefinition.BlockStatement = ruleDefinition.SwitchStatement = findVariables;

        ruleDefinition.ArrowFunctionExpression = function(node) {
            if (node.body.type !== "BlockStatement") {
                findVariables(node);
            }
        };
    } else {
        ruleDefinition.FunctionExpression = ruleDefinition.FunctionDeclaration = ruleDefinition.ArrowFunctionExpression = findVariables;
    }

    return ruleDefinition;
};

module.exports.schema = [
    {
        "enum": ["nofunc"]
    }
];
