/**
 * @fileoverview Rule to flag unnecessary strict directives.
 * @author Ian Christian Myers
 * @copyright 2014 Ian Christian Myers. All rights reserved.
 */
"use strict";

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {

    function directives(block) {
        var ds = [], body = block.body, e, i, l;

        if (body) {
            for (i = 0, l = body.length; i < l; ++i) {
                e = body[i];

                if (
                    e.type === "ExpressionStatement" &&
                    e.expression.type === "Literal" &&
                    typeof e.expression.value === "string"
                ) {
                    ds.push(e.expression);
                } else {
                    break;
                }
            }
        }

        return ds;
    }

    function isStrict(directive) {
        return directive.value === "use strict";
    }

    function checkForUnnecessaryUseStrict(node) {
        var useStrictDirectives = directives(node).filter(isStrict),
            scope,
            upper;

        switch (useStrictDirectives.length) {
            case 0:
                break;

            case 1:
                scope = context.getScope();
                upper = scope.upper;

                if (upper && upper.functionExpressionScope) {
                    upper = upper.upper;
                }

                if (upper && upper.isStrict) {
                    context.report(useStrictDirectives[0], "Unnecessary 'use strict'.");
                }
                break;

            default:
                context.report(useStrictDirectives[1], "Multiple 'use strict' directives.");
        }
    }

    return {

        "Program": checkForUnnecessaryUseStrict,

        "ArrowFunctionExpression": function(node) {
            checkForUnnecessaryUseStrict(node.body);
        },

        "FunctionExpression": function(node) {
            checkForUnnecessaryUseStrict(node.body);
        },

        "FunctionDeclaration": function(node) {
            checkForUnnecessaryUseStrict(node.body);
        }
    };

};

module.exports.schema = [];
