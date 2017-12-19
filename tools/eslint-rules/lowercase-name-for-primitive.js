/**
 * @fileoverview Check that TypeError[ERR_INVALID_ARG_TYPE] uses
 *               lowercase for primitive types
 * @author Weijia Wang <starkwang@126.com>
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const astSelector = 'NewExpression[callee.property.name="TypeError"]' +
                    '[arguments.0.value="ERR_INVALID_ARG_TYPE"]';

const primitives = [
  'number', 'string', 'boolean', 'null', 'undefined'
];

module.exports = function(context) {
  function checkNamesArgument(node) {
    const names = node.arguments[2];

    switch (names.type) {
      case 'Literal':
        checkName(names);
        break;
      case 'ArrayExpression':
        names.elements.forEach((name) => {
          checkName(name);
        });
        break;
    }
  }

  function checkName(node) {
    const name = node.value;
    const lowercaseName = name.toLowerCase();
    if (name !== lowercaseName && primitives.includes(lowercaseName)) {
      const msg = `primitive should use lowercase: ${name}`;
      context.report({
        node,
        message: msg,
        fix: (fixer) => {
          return fixer.replaceText(
            node,
            `'${lowercaseName}'`
          );
        }
      });
    }

  }

  return {
    [astSelector]: (node) => checkNamesArgument(node)
  };
};
