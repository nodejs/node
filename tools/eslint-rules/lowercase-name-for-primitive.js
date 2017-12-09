/**
 * @fileoverview Check that TypeError[ERR_INVALID_ARG_TYPE] uses
 *               lowercase for primitive types
 * @author Weijia Wang <starkwang@126.com>
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const primitives = [
  'number', 'string', 'boolean', 'null', 'undefined'
];

module.exports = function(context) {
  return {
    NewExpression(node) {
      if (
        node.callee.property &&
        node.callee.property.name === 'TypeError' &&
        node.arguments[0].value === 'ERR_INVALID_ARG_TYPE'
      ) {
        checkNamesArgument(node.arguments[2]);
      }

      function checkNamesArgument(names) {
        switch (names.type) {
          case 'Literal':
            checkName(names.value);
            break;
          case 'ArrayExpression':
            names.elements.forEach((name) => {
              checkName(name.value);
            });
            break;
        }
      }

      function checkName(name) {
        const lowercaseName = name.toLowerCase();
        if (primitives.includes(lowercaseName) && !primitives.includes(name)) {
          const msg = `primitive should use lowercase: ${name}`;
          context.report(node, msg);
        }
      }
    }
  };
};
