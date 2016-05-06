/*
  Copyright (C) 2012 Yusuke Suzuki <utatane.tea@gmail.com>

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*jshint eqeqeq:false*/
(function () {
    'use strict';

    var Syntax, common;

    common = require('./common');
    Syntax = common.Syntax;

    // constant

    function isConstant(node, allowRegExp) {
        if (node.type === Syntax.Literal) {
            if (typeof node.value === 'object' && node.value !== null) {
                // This is RegExp
                return allowRegExp;
            }
            return true;
        }
        if (node.type === Syntax.UnaryExpression) {
            if (node.operator === 'void' || node.operator === 'delete' || node.operator === '!') {
                return isConstant(node.argument, true);
            }
            return isConstant(node.argument, false);
        }
        if (node.type === Syntax.BinaryExpression) {
            if (node.operator === 'in' || node.operator === 'instanceof') {
                return false;
            }
            return isConstant(node.left, false) && isConstant(node.right, false);
        }
        if (node.type === Syntax.LogicalExpression) {
            return isConstant(node.left, true) && isConstant(node.right, true);
        }
        return false;
    }

    function getConstant(node) {
        if (node.type === Syntax.Literal) {
            return node.value;
        }
        if (node.type === Syntax.UnaryExpression) {
            return doUnary(node.operator, getConstant(node.argument));
        }
        if (node.type === Syntax.BinaryExpression) {
            return doBinary(node.operator, getConstant(node.left), getConstant(node.right));
        }
        if (node.type === Syntax.LogicalExpression) {
            return doLogical(node.operator, getConstant(node.left), getConstant(node.right));
        }
        common.unreachable();
    }

    function doLogical(operator, left, right) {
        if (operator === '||') {
            return left || right;
        }
        if (operator === '&&') {
            return left && right;
        }
        common.unreachable();
    }

    function doUnary(operator, argument) {
        switch (operator) {
        case '+':
            return +argument;
        case '-':
            return -argument;
        case '~':
            return ~argument;
        case '!':
            return !argument;
        case 'delete':
            // do delete on constant value (not considering identifier in this tree based constant folding)
            return true;
        case 'void':
            return undefined;
        case 'typeof':
            return typeof argument;
        }
        common.unreachable();
    }

    function doBinary(operator, left, right) {
        switch (operator) {
        case '|':
            return left | right;
        case '^':
            return left ^ right;
        case '&':
            return left & right;
        case '==':
            return left == right;
        case '!=':
            return left != right;
        case '===':
            return left === right;
        case '!==':
            return left !== right;
        case '<':
            return left < right;
        case '>':
            return left > right;
        case '<=':
            return left <= right;
        case '>=':
            return left >= right;
        // case 'in':
        //    return left in right;
        // case 'instanceof':
        //    return left instanceof right;
        case '<<':
            return left << right;
        case '>>':
            return left >> right;
        case '>>>':
            return left >>> right;
        case '+':
            return left + right;
        case '-':
            return left - right;
        case '*':
            return left * right;
        case '/':
            return left / right;
        case '%':
            return left % right;
        }
        common.unreachable();
    }

    exports.constant = {
        doBinary: doBinary,
        doUnary: doUnary,
        doLogical: doLogical,
        evaluate: getConstant,
        isConstant: isConstant
    };

    // has side effect
    function hasSideEffect(expr, scope) {
        function visit(expr) {
            var i, iz, ref;
            switch (expr.type) {
            case Syntax.AssignmentExpression:
                return true;

            case Syntax.ArrayExpression:
                for (i = 0, iz = expr.elements.length; i < iz; ++i) {
                    if (expr.elements[i] !== null && visit(expr.elements[i])) {
                        return true;
                    }
                }
                return false;

            case Syntax.BinaryExpression:
                return !isConstant(expr);

            case Syntax.CallExpression:
                return true;

            case Syntax.ConditionalExpression:
                return visit(expr.test) || visit(expr.consequent) || visit(expr.alternate);

            case Syntax.FunctionExpression:
                return false;

            case Syntax.Identifier:
                ref = scope.resolve(expr);
                if (ref && ref.isStatic()) {
                    return false;
                }
                return true;

            case Syntax.Literal:
                return false;

            case Syntax.LogicalExpression:
                return visit(expr.left) || visit(expr.right);

            case Syntax.MemberExpression:
                return true;

            case Syntax.NewExpression:
                return true;

            case Syntax.ObjectExpression:
                for (i = 0, iz = expr.properties.length; i < iz; ++i) {
                    if (visit(expr.properties[i])) {
                        return true;
                    }
                }
                return false;

            case Syntax.Property:
                return visit(expr.value);

            case Syntax.SequenceExpression:
                for (i = 0, iz = expr.expressions.length; i < iz; ++i) {
                    if (visit(expr.expressions[i])) {
                        return true;
                    }
                }
                return false;

            case Syntax.ThisExpression:
                return false;

            case Syntax.UnaryExpression:
                if (expr.operator === 'void' || expr.operator === 'delete' || expr.operator === 'typeof' || expr.operator === '!') {
                    return visit(expr.argument);
                }
                return !isConstant(expr);

            case Syntax.UpdateExpression:
                return true;
            }
            return true;
        }

        return visit(expr);
    }

    exports.hasSideEffect = hasSideEffect;

    // boolean decision
    // @return {boolean|null} when indeterminate value comes, returns null
    function booleanCondition(expr) {
        var ret;
        switch (expr.type) {
        case Syntax.AssignmentExpression:
            return booleanCondition(expr.right);

        case Syntax.ArrayExpression:
            return true;

        case Syntax.BinaryExpression:
            if (isConstant(expr)) {
                return !!getConstant(expr);
            }
            return null;

        case Syntax.CallExpression:
            return null;

        case Syntax.ConditionalExpression:
            ret = booleanCondition(expr.test);
            if (ret === true) {
                return booleanCondition(expr.consequent);
            }
            if (ret === false) {
                return booleanCondition(expr.alternate);
            }
            ret = booleanCondition(expr.consequent);
            if (ret === booleanCondition(expr.alternate)) {
                return ret;
            }
            return null;

        case Syntax.FunctionExpression:
            return true;

        case Syntax.Identifier:
            return null;

        case Syntax.Literal:
            return !!getConstant(expr);

        case Syntax.LogicalExpression:
            if (expr.operator === '&&') {
                ret = booleanCondition(expr.left);
                if (ret === null) {
                    return null;
                }
                if (!ret) {
                    return false;
                }
                return booleanCondition(expr.right);
            } else {
                ret = booleanCondition(expr.left);
                if (ret === null) {
                    return null;
                }
                if (ret) {
                    return true;
                }
                return booleanCondition(expr.right);
            }
            return null;

        case Syntax.MemberExpression:
            return null;

        case Syntax.NewExpression:
            // always return object
            return true;

        case Syntax.ObjectExpression:
            return true;

        case Syntax.Property:
            common.unreachable();
            return null;

        case Syntax.SequenceExpression:
            return booleanCondition(common.Array.last(expr.expressions));

        case Syntax.ThisExpression:
            // in strict mode, this may be null / undefined
            return null;

        case Syntax.UnaryExpression:
            if (expr.operator === 'void') {
                return false;
            }
            if (expr.operator === 'typeof') {
                return true;
            }
            if (expr.operator === '!') {
                ret = booleanCondition(expr.argument);
                if (ret === null) {
                    return null;
                }
                return !ret;
            }
            if (isConstant(expr)) {
                return !!getConstant(expr);
            }
            return null;

        case Syntax.UpdateExpression:
            return null;
        }

        return null;
    }

    exports.booleanCondition = booleanCondition;
}());
