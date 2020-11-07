/**
 * @fileoverview We shouldn't use global built-in object for security and
 *               performance reason. This linter rule reports replacable codes
 *               that can be replaced with primordials.
 * @author Leko <leko.noor@gmail.com>
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

function toPrimordialsName(obj, prop) {
  return obj + toUcFirst(prop);
}

function toUcFirst(str) {
  return str[0].toUpperCase() + str.slice(1);
}

function isTarget(map, varName) {
  return map.has(varName);
}

function isIgnored(map, varName, propName) {
  if (!map.has(varName) || !map.get(varName).has(propName)) {
    return false;
  }
  return map.get(varName).get(propName).ignored;
}

function getReportName({ name, parentName, into }) {
  if (into) {
    return toPrimordialsName(into, name);
  }
  if (parentName) {
    return toPrimordialsName(parentName, name);
  }
  return name;
}

/**
 * Get identifier of object spread assignment
 *
 * code: 'const { ownKeys } = Reflect;'
 * argument: 'ownKeys'
 * return: 'Reflect'
 */
function getDestructuringAssignmentParent(scope, node) {
  const declaration = scope.set.get(node.name);
  if (
    !declaration ||
    !declaration.defs ||
    declaration.defs.length === 0 ||
    declaration.defs[0].type !== 'Variable' ||
    !declaration.defs[0].node.init
  ) {
    return null;
  }
  return declaration.defs[0].node.init.name;
}

const identifierSelector =
  '[type!=VariableDeclarator][type!=MemberExpression]>Identifier';

module.exports = {
  meta: {
    messages: {
      error: 'Use `const { {{name}} } = primordials;` instead of the global.'
    }
  },
  create(context) {
    const globalScope = context.getSourceCode().scopeManager.globalScope;
    const nameMap = context.options.reduce((acc, option) =>
      acc.set(
        option.name,
        (option.ignore || [])
          .reduce((acc, name) => acc.set(name, {
            ignored: true
          }), new Map())
      )
    , new Map());
    const renameMap = context.options
      .filter((option) => option.into)
      .reduce((acc, option) =>
        acc.set(option.name, option.into)
      , new Map());
    let reported;

    return {
      Program() {
        reported = new Map();
      },
      [identifierSelector](node) {
        if (reported.has(node.range[0])) {
          return;
        }
        const name = node.name;
        const parentName = getDestructuringAssignmentParent(
          context.getScope(),
          node
        );
        if (!isTarget(nameMap, name) && !isTarget(nameMap, parentName)) {
          return;
        }

        const defs = (globalScope.set.get(name) || {}).defs || null;
        if (parentName && isTarget(nameMap, parentName)) {
          if (!defs || defs[0].name.name !== 'primordials') {
            reported.set(node.range[0], true);
            const into = renameMap.get(name);
            context.report({
              node,
              messageId: 'error',
              data: {
                name: getReportName({ into, parentName, name })
              }
            });
          }
          return;
        }
        if (defs.length === 0 || defs[0].node.init.name !== 'primordials') {
          reported.set(node.range[0], true);
          const into = renameMap.get(name);
          context.report({
            node,
            messageId: 'error',
            data: {
              name: getReportName({ into, parentName, name })
            }
          });
        }
      },
      MemberExpression(node) {
        const obj = node.object.name;
        const prop = node.property.name;
        if (!prop || !isTarget(nameMap, obj) || isIgnored(nameMap, obj, prop)) {
          return;
        }

        const variables =
          context.getSourceCode().scopeManager.getDeclaredVariables(node);
        if (variables.length === 0) {
          context.report({
            node,
            messageId: 'error',
            data: {
              name: toPrimordialsName(obj, prop),
            }
          });
        }
      }
    };
  }
};
