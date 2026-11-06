'use strict';

const CallExpression = (fnName) => `CallExpression[callee.name=${fnName}]`;
const AnyFunction = 'FunctionDeclaration, FunctionExpression, ArrowFunctionExpression';

function checkProperties(context, node) {
  if (
    node.type === 'CallExpression' &&
    node.callee.name === 'ObjectGetOwnPropertyDescriptors'
  ) {
    context.report({
      node,
      message:
        'Property descriptors inherits from the Object prototype, therefore are subject to prototype pollution',
    });
  }
  if (node.type !== 'ObjectExpression') return;
  for (const { key, value } of node.properties) {
    if (key != null && value != null && !isProtoKey(key)) {
      checkPropertyDescriptor(context, value);
    }
  }
}

function isProtoKey(key) {
  return key != null &&
    ((key.type === 'Identifier' && key.name === '__proto__') ||
      (key.type === 'Literal' && key.value === '__proto__'));
}

function findVariable(context, identifier) {
  let scope = context.sourceCode.getScope(identifier);
  while (scope != null) {
    const variable = scope.set.get(identifier.name);
    if (variable != null) return variable;
    scope = scope.upper;
  }
  return null;
}

// Frozen null-prototype objects exported from internal modules that can be
// safely used as the prototype of an object literal.
const knownNullPrototypeExports = {
  'internal/util': ['kEmptyObject'],
};

function isRequireOfKnownNullPrototypeExport(declarator, name) {
  const { id, init } = declarator;
  if (
    id.type !== 'ObjectPattern' ||
    init == null ||
    init.type !== 'CallExpression' ||
    init.callee.type !== 'Identifier' ||
    init.callee.name !== 'require' ||
    init.arguments.length !== 1 ||
    init.arguments[0].type !== 'Literal'
  ) {
    return false;
  }
  const allowed = knownNullPrototypeExports[init.arguments[0].value];
  if (allowed == null) return false;
  return id.properties.some((property) =>
    property.type === 'Property' &&
    !property.computed &&
    property.key.type === 'Identifier' &&
    allowed.includes(property.key.name) &&
    property.value.type === 'Identifier' &&
    property.value.name === name,
  );
}

// Returns true if `node` is an expression that is known to evaluate to a
// frozen object whose prototype is null, e.g.:
//   const kNullProtoObject = ObjectFreeze({ __proto__: null });
// Using such an object as the `__proto__` of an object literal is equivalent
// to `__proto__: null` in terms of protection against prototype pollution, but
// avoids V8 creating the object literal in dictionary mode.
function isFrozenNullPrototypeObject(context, node, seen = new Set()) {
  if (node.type === 'Identifier') {
    const variable = findVariable(context, node);
    if (variable == null || seen.has(variable) || variable.defs.length !== 1) {
      return false;
    }
    seen.add(variable);
    const [def] = variable.defs;
    if (
      def.type !== 'Variable' ||
      def.parent.kind !== 'const' ||
      def.node.init == null
    ) {
      return false;
    }
    if (def.node.id.type === 'Identifier') {
      return isFrozenNullPrototypeObject(context, def.node.init, seen);
    }
    return isRequireOfKnownNullPrototypeExport(def.node, node.name);
  }

  if (
    node.type === 'CallExpression' &&
    node.callee.type === 'Identifier' &&
    node.callee.name === 'ObjectFreeze' &&
    node.arguments.length === 1
  ) {
    const [arg] = node.arguments;
    if (
      arg.type === 'CallExpression' &&
      arg.callee.type === 'Identifier' &&
      arg.callee.name === 'ObjectCreate' &&
      arg.arguments.length === 1 &&
      arg.arguments[0].type === 'Literal' &&
      arg.arguments[0].value === null
    ) {
      return true;
    }
    return isNullPrototypeObjectExpression(context, arg, seen);
  }

  return false;
}

function isNullPrototypeObjectExpression(context, node, seen) {
  if (node.type !== 'ObjectExpression') return;

  for (const { key, value } of node.properties) {
    if (isProtoKey(key) && value != null) {
      if (value.type === 'Literal' && value.value === null) {
        return true;
      }
      if (isFrozenNullPrototypeObject(context, value, seen)) {
        return true;
      }
    }
  }
  return false;
}

function checkPropertyDescriptor(context, node) {
  if (
    node.type === 'CallExpression' &&
    (node.callee.name === 'ObjectGetOwnPropertyDescriptor' ||
     node.callee.name === 'ReflectGetOwnPropertyDescriptor')
  ) {
    context.report({
      node,
      message:
        'Property descriptors inherits from the Object prototype, therefore are subject to prototype pollution',
      suggest: [{
        desc: 'Wrap the property descriptor in a null-prototype object',
        fix(fixer) {
          return [
            fixer.insertTextBefore(node, '{ __proto__: null,...'),
            fixer.insertTextAfter(node, ' }'),
          ];
        },
      }],
    });
  }
  if (isNullPrototypeObjectExpression(context, node) === false) {
    context.report({
      node,
      message: 'Must use null-prototype object for property descriptors',
    });
  }
}

function createUnsafeStringMethodReport(context, name, lookedUpProperty) {
  const lastDotPosition = '$String.prototype.'.length;
  const unsafePrimordialName = `StringPrototype${name.charAt(lastDotPosition).toUpperCase()}${name.slice(lastDotPosition + 1, -1)}`;
  return {
    [CallExpression(unsafePrimordialName)](node) {
      context.report({
        node,
        message: `${name} looks up the ${lookedUpProperty} property on the first argument`,
      });
    },
  };
}

function createUnsafeStringMethodOnRegexReport(context, name, lookedUpProperty) {
  const dotPosition = 'Symbol.'.length;
  const safePrimordialName = `RegExpPrototypeSymbol${lookedUpProperty.charAt(dotPosition).toUpperCase()}${lookedUpProperty.slice(dotPosition + 1)}`;
  const lastDotPosition = '$String.prototype.'.length;
  const unsafePrimordialName = `StringPrototype${name.charAt(lastDotPosition).toUpperCase()}${name.slice(lastDotPosition + 1, -1)}`;
  return {
    [[
      `${CallExpression(unsafePrimordialName)}[arguments.1.type=Literal][arguments.1.regex]`,
      `${CallExpression(unsafePrimordialName)}[arguments.1.type=NewExpression][arguments.1.callee.name=RegExp]`,
    ].join(',')](node) {
      context.report({
        node,
        message: `${name} looks up the ${lookedUpProperty} property of the passed regex, use ${safePrimordialName} directly`,
      });
    },
  };
}

module.exports = {
  meta: { hasSuggestions: true },
  create(context) {
    return {
      [CallExpression(/^(Object|Reflect)DefinePropert(ies|y)$/)](node) {
        switch (node.callee.name) {
          case 'ObjectDefineProperties':
            checkProperties(context, node.arguments[1]);
            break;
          case 'ReflectDefineProperty':
          case 'ObjectDefineProperty':
            checkPropertyDescriptor(context, node.arguments[2]);
            break;
          default:
            throw new Error('Unreachable');
        }
      },

      [`${CallExpression('ObjectCreate')}[arguments.length=2]`](node) {
        checkProperties(context, node.arguments[1]);
      },

      [`:matches(${AnyFunction})[async=true]>BlockStatement ReturnStatement>ObjectExpression, :matches(${AnyFunction})[async=true]>ObjectExpression`](node) {
        if (node.parent.type === 'ReturnStatement') {
          let { parent } = node;
          do {
            ({ parent } = parent);
          } while (!parent.type.includes('Function'));

          if (!parent.async) return;
        }
        if (isNullPrototypeObjectExpression(context, node) === false) {
          context.report({
            node,
            message: 'Use null-prototype when returning from async function',
          });
        }
      },

      [CallExpression('RegExpPrototypeTest')](node) {
        context.report({
          node,
          message: '%RegExp.prototype.test% looks up the "exec" property of `this` value',
          suggest: [{
            desc: 'Use RegexpPrototypeExec instead',
            fix(fixer) {
              const testRange = [ ...node.range ];
              testRange[0] = testRange[0] + 'RegexpPrototype'.length;
              testRange[1] = testRange[0] + 'Test'.length;
              return [
                fixer.replaceTextRange(testRange, 'Exec'),
                fixer.insertTextAfter(node, ' !== null'),
              ];
            },
          }],
        });
      },
      [CallExpression(/^RegExpPrototypeSymbol(Match|MatchAll)$/)](node) {
        context.report({
          node,
          message: node.callee.name + ' looks up the "exec" property of `this` value',
        });
      },
      [CallExpression(/^(RegExpPrototypeSymbol|StringPrototype)Search$/)](node) {
        context.report({
          node,
          message: node.callee.name + ' is unsafe, use SafeStringPrototypeSearch instead',
        });
      },
      ...createUnsafeStringMethodReport(context, '%String.prototype.match%', 'Symbol.match'),
      ...createUnsafeStringMethodReport(context, '%String.prototype.matchAll%', 'Symbol.matchAll'),
      ...createUnsafeStringMethodOnRegexReport(context, '%String.prototype.replace%', 'Symbol.replace'),
      ...createUnsafeStringMethodOnRegexReport(context, '%String.prototype.replaceAll%', 'Symbol.replace'),
      ...createUnsafeStringMethodOnRegexReport(context, '%String.prototype.split%', 'Symbol.split'),

      'NewExpression[callee.name="Proxy"][arguments.1.type="ObjectExpression"]'(node) {
        if (isNullPrototypeObjectExpression(context, node.arguments[1])) return;
        context.report({
          node,
          message: 'Proxy handler must be a null-prototype object',
        });
      },

      [`ExpressionStatement>AwaitExpression>${CallExpression(/^(Safe)?PromiseAll(Settled)?$/)}`](node) {
        context.report({
          node,
          message: `Use ${node.callee.name}ReturnVoid`,
        });
      },

      [CallExpression('PromisePrototypeCatch')](node) {
        context.report({
          node,
          message: '%Promise.prototype.catch% looks up the `then` property of ' +
                   'the `this` argument, use PromisePrototypeThen instead',
        });
      },

      [CallExpression('PromisePrototypeFinally')](node) {
        context.report({
          node,
          message: '%Promise.prototype.finally% looks up the `then` property of ' +
                   'the `this` argument, use SafePromisePrototypeFinally or ' +
                   'try/finally instead',
        });
      },

      [CallExpression(/^Promise(All(Settled)?|Any|Race)/)](node) {
        context.report({
          node,
          message: `Use Safe${node.callee.name} instead of ${node.callee.name}`,
        });
      },

      [CallExpression('ArrayPrototypeConcat')](node) {
        context.report({
          node,
          message: '%Array.prototype.concat% looks up `@@isConcatSpreadable` ' +
                   'which can be subject to prototype pollution',
        });
      },

      'AssignmentPattern[right.type="ObjectExpression"]'(node) {
        if (!node.right.properties.length) {
          context.report({
            node: node.right,
            message: 'Use kEmptyObject instead of declaring a new empty object, or define a __proto__ property',
          });
          return;
        }
        const propertyIsIdentifier = (p) => p.key.type === 'Identifier';
        if (node.left.type === 'ObjectPattern' &&
            node.left.properties.every(propertyIsIdentifier) &&
            node.right.properties.every(propertyIsIdentifier)) {
          const rightNames = node.right.properties.map((p) => p.key.name);
          if (node.left.properties.every((p) => rightNames.includes(p.key.name))) return;
        }
        if (node.right.properties.some((p) => p.key.name === '__proto__')) return;
        context.report({
          node: node.right,
          message: `Add '__proto__: null' to avoid inheriting from Object.prototype, or '__proto__: ObjectPrototype' if inheritance is desirable`,
        });
      },
    };
  },
};
