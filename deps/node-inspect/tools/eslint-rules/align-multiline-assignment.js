/**
 * @fileoverview Align multiline variable assignments
 * @author Rich Trott
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
function getBinaryExpressionStarts(binaryExpression, starts) {
  function getStartsFromOneSide(side, starts) {
    starts.push(side.loc.start);
    if (side.type === 'BinaryExpression') {
      starts = getBinaryExpressionStarts(side, starts);
    }
    return starts;
  }

  starts = getStartsFromOneSide(binaryExpression.left, starts);
  starts = getStartsFromOneSide(binaryExpression.right, starts);
  return starts;
}

function checkExpressionAlignment(expression) {
  if (!expression)
    return;

  var msg = '';

  switch (expression.type) {
    case 'BinaryExpression':
      var starts = getBinaryExpressionStarts(expression, []);
      var startLine = starts[0].line;
      const startColumn = starts[0].column;
      starts.forEach((loc) => {
        if (loc.line > startLine) {
          startLine = loc.line;
          if (loc.column !== startColumn) {
            msg = 'Misaligned multiline assignment';
          }
        }
      });
      break;
  }
  return msg;
}

function testAssignment(context, node) {
  const msg = checkExpressionAlignment(node.right);
  if (msg)
    context.report(node, msg);
}

function testDeclaration(context, node) {
  node.declarations.forEach((declaration) => {
    const msg = checkExpressionAlignment(declaration.init);
    // const start = declaration.init.loc.start;
    if (msg)
      context.report(node, msg);
  });
}

module.exports = function(context) {
  return {
    'AssignmentExpression': (node) => testAssignment(context, node),
    'VariableDeclaration': (node) => testDeclaration(context, node)
  };
};
