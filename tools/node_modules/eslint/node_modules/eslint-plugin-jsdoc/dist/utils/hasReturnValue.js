"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.hasValueOrExecutorHasNonEmptyResolveValue = exports.hasReturnValue = void 0;

/* eslint-disable jsdoc/no-undefined-types */

/**
 * Checks if a node is a promise but has no resolve value or an empty value.
 * An `undefined` resolve does not count.
 *
 * @param {object} node
 * @returns {boolean}
 */
const isNewPromiseExpression = node => {
  return node && node.type === 'NewExpression' && node.callee.type === 'Identifier' && node.callee.name === 'Promise';
};

const isVoidPromise = node => {
  var _node$typeParameters, _node$typeParameters$, _node$typeParameters$2;

  return (node === null || node === void 0 ? void 0 : (_node$typeParameters = node.typeParameters) === null || _node$typeParameters === void 0 ? void 0 : (_node$typeParameters$ = _node$typeParameters.params) === null || _node$typeParameters$ === void 0 ? void 0 : (_node$typeParameters$2 = _node$typeParameters$[0]) === null || _node$typeParameters$2 === void 0 ? void 0 : _node$typeParameters$2.type) === 'TSVoidKeyword';
};
/**
 * @callback PromiseFilter
 * @param {object} node
 * @returns {boolean}
 */

/**
 * Checks if a node has a return statement. Void return does not count.
 *
 * @param {object} node
 * @param {PromiseFilter} promFilter
 * @returns {boolean|Node}
 */
// eslint-disable-next-line complexity


const hasReturnValue = (node, promFilter) => {
  var _node$returnType, _node$returnType$type;

  if (!node) {
    return false;
  }

  switch (node.type) {
    case 'TSFunctionType':
    case 'TSMethodSignature':
      return !['TSVoidKeyword', 'TSUndefinedKeyword'].includes(node === null || node === void 0 ? void 0 : (_node$returnType = node.returnType) === null || _node$returnType === void 0 ? void 0 : (_node$returnType$type = _node$returnType.typeAnnotation) === null || _node$returnType$type === void 0 ? void 0 : _node$returnType$type.type);

    case 'MethodDefinition':
      return hasReturnValue(node.value, promFilter);

    case 'FunctionExpression':
    case 'FunctionDeclaration':
    case 'ArrowFunctionExpression':
      {
        return node.expression && (!isNewPromiseExpression(node.body) || !isVoidPromise(node.body)) || hasReturnValue(node.body, promFilter);
      }

    case 'BlockStatement':
      {
        return node.body.some(bodyNode => {
          return bodyNode.type !== 'FunctionDeclaration' && hasReturnValue(bodyNode, promFilter);
        });
      }

    case 'LabeledStatement':
    case 'WhileStatement':
    case 'DoWhileStatement':
    case 'ForStatement':
    case 'ForInStatement':
    case 'ForOfStatement':
    case 'WithStatement':
      {
        return hasReturnValue(node.body, promFilter);
      }

    case 'IfStatement':
      {
        return hasReturnValue(node.consequent, promFilter) || hasReturnValue(node.alternate, promFilter);
      }

    case 'TryStatement':
      {
        return hasReturnValue(node.block, promFilter) || hasReturnValue(node.handler && node.handler.body, promFilter) || hasReturnValue(node.finalizer, promFilter);
      }

    case 'SwitchStatement':
      {
        return node.cases.some(someCase => {
          return someCase.consequent.some(nde => {
            return hasReturnValue(nde, promFilter);
          });
        });
      }

    case 'ReturnStatement':
      {
        // void return does not count.
        if (node.argument === null) {
          return false;
        }

        if (promFilter && isNewPromiseExpression(node.argument)) {
          // Let caller decide how to filter, but this is, at the least,
          //   a return of sorts and truthy
          return promFilter(node.argument);
        }

        return true;
      }

    default:
      {
        return false;
      }
  }
};
/**
 * Avoids further checking child nodes if a nested function shadows the
 * resolver, but otherwise, if name is used (by call or passed in as an
 * argument to another function), will be considered as non-empty.
 *
 * This could check for redeclaration of the resolver, but as such is
 * unlikely, we avoid the performance cost of checking everywhere for
 * (re)declarations or assignments.
 *
 * @param {AST} node
 * @param {string} resolverName
 * @returns {boolean}
 */
// eslint-disable-next-line complexity


exports.hasReturnValue = hasReturnValue;

const hasNonEmptyResolverCall = (node, resolverName) => {
  if (!node) {
    return false;
  } // Arrow function without block


  switch (node.type) {
    // istanbul ignore next -- In Babel?
    case 'OptionalCallExpression':
    case 'CallExpression':
      return node.callee.name === resolverName && ( // Implicit or explicit undefined
      node.arguments.length > 1 || node.arguments[0] !== undefined) || node.arguments.some(nde => {
        // Being passed in to another function (which might invoke it)
        return nde.type === 'Identifier' && nde.name === resolverName || // Handle nested items
        hasNonEmptyResolverCall(nde, resolverName);
      });

    case 'ChainExpression':
    case 'Decorator':
    case 'ExpressionStatement':
      return hasNonEmptyResolverCall(node.expression, resolverName);

    case 'ClassBody':
    case 'BlockStatement':
      return node.body.some(bodyNode => {
        return hasNonEmptyResolverCall(bodyNode, resolverName);
      });

    case 'FunctionExpression':
    case 'FunctionDeclaration':
    case 'ArrowFunctionExpression':
      {
        var _node$params$;

        // Shadowing
        if (((_node$params$ = node.params[0]) === null || _node$params$ === void 0 ? void 0 : _node$params$.name) === resolverName) {
          return false;
        }

        return hasNonEmptyResolverCall(node.body, resolverName);
      }

    case 'LabeledStatement':
    case 'WhileStatement':
    case 'DoWhileStatement':
    case 'ForStatement':
    case 'ForInStatement':
    case 'ForOfStatement':
    case 'WithStatement':
      {
        return hasNonEmptyResolverCall(node.body, resolverName);
      }

    case 'ConditionalExpression':
    case 'IfStatement':
      {
        return hasNonEmptyResolverCall(node.test, resolverName) || hasNonEmptyResolverCall(node.consequent, resolverName) || hasNonEmptyResolverCall(node.alternate, resolverName);
      }

    case 'TryStatement':
      {
        return hasNonEmptyResolverCall(node.block, resolverName) || hasNonEmptyResolverCall(node.handler && node.handler.body, resolverName) || hasNonEmptyResolverCall(node.finalizer, resolverName);
      }

    case 'SwitchStatement':
      {
        return node.cases.some(someCase => {
          return someCase.consequent.some(nde => {
            return hasNonEmptyResolverCall(nde, resolverName);
          });
        });
      }

    case 'ArrayPattern':
    case 'ArrayExpression':
      return node.elements.some(element => {
        return hasNonEmptyResolverCall(element, resolverName);
      });

    case 'AssignmentPattern':
      return hasNonEmptyResolverCall(node.right, resolverName);

    case 'AssignmentExpression':
    case 'BinaryExpression':
    case 'LogicalExpression':
      {
        return hasNonEmptyResolverCall(node.left, resolverName) || hasNonEmptyResolverCall(node.right, resolverName);
      }
    // Comma

    case 'SequenceExpression':
    case 'TemplateLiteral':
      return node.expressions.some(subExpression => {
        return hasNonEmptyResolverCall(subExpression, resolverName);
      });

    case 'ObjectPattern':
    case 'ObjectExpression':
      return node.properties.some(property => {
        return hasNonEmptyResolverCall(property, resolverName);
      });
    // istanbul ignore next -- In Babel?

    case 'ClassMethod':
    case 'MethodDefinition':
      return node.decorators && node.decorators.some(decorator => {
        return hasNonEmptyResolverCall(decorator, resolverName);
      }) || node.computed && hasNonEmptyResolverCall(node.key, resolverName) || hasNonEmptyResolverCall(node.value, resolverName);
    // istanbul ignore next -- In Babel?

    case 'ObjectProperty':
    /* eslint-disable no-fallthrough */
    // istanbul ignore next -- In Babel?

    case 'PropertyDefinition': // istanbul ignore next -- In Babel?

    case 'ClassProperty':
    /* eslint-enable no-fallthrough */

    case 'Property':
      return node.computed && hasNonEmptyResolverCall(node.key, resolverName) || hasNonEmptyResolverCall(node.value, resolverName);
    // istanbul ignore next -- In Babel?

    case 'ObjectMethod':
      // istanbul ignore next -- In Babel?
      return node.computed && hasNonEmptyResolverCall(node.key, resolverName) || node.arguments.some(nde => {
        return hasNonEmptyResolverCall(nde, resolverName);
      });

    case 'ClassExpression':
    case 'ClassDeclaration':
      return hasNonEmptyResolverCall(node.body, resolverName);

    case 'AwaitExpression':
    case 'SpreadElement':
    case 'UnaryExpression':
    case 'YieldExpression':
      return hasNonEmptyResolverCall(node.argument, resolverName);

    case 'VariableDeclaration':
      {
        return node.declarations.some(nde => {
          return hasNonEmptyResolverCall(nde, resolverName);
        });
      }

    case 'VariableDeclarator':
      {
        return hasNonEmptyResolverCall(node.id, resolverName) || hasNonEmptyResolverCall(node.init, resolverName);
      }

    case 'TaggedTemplateExpression':
      return hasNonEmptyResolverCall(node.quasi, resolverName);
    // ?.
    // istanbul ignore next -- In Babel?

    case 'OptionalMemberExpression':
    case 'MemberExpression':
      return hasNonEmptyResolverCall(node.object, resolverName) || hasNonEmptyResolverCall(node.property, resolverName);
    // istanbul ignore next -- In Babel?

    case 'Import':
    case 'ImportExpression':
      return hasNonEmptyResolverCall(node.source, resolverName);

    case 'ReturnStatement':
      {
        if (node.argument === null) {
          return false;
        }

        return hasNonEmptyResolverCall(node.argument, resolverName);
      }

    /*
    // Shouldn't need to parse literals/literal components, etc.
     case 'Identifier':
    case 'TemplateElement':
    case 'Super':
    // Exports not relevant in this context
    */

    default:
      return false;
  }
};
/**
 * Checks if a Promise executor has no resolve value or an empty value.
 * An `undefined` resolve does not count.
 *
 * @param {object} node
 * @param {boolean} anyPromiseAsReturn
 * @returns {boolean}
 */


const hasValueOrExecutorHasNonEmptyResolveValue = (node, anyPromiseAsReturn) => {
  return hasReturnValue(node, prom => {
    if (anyPromiseAsReturn) {
      return true;
    }

    if (isVoidPromise(prom)) {
      return false;
    }

    const [{
      params,
      body
    } = {}] = prom.arguments;

    if (!(params !== null && params !== void 0 && params.length)) {
      return false;
    }

    const [{
      name: resolverName
    }] = params;
    return hasNonEmptyResolverCall(body, resolverName);
  });
};

exports.hasValueOrExecutorHasNonEmptyResolveValue = hasValueOrExecutorHasNonEmptyResolveValue;
//# sourceMappingURL=hasReturnValue.js.map