/**
 * @fileoverview Align arguments in multiline function calls
 * @author Rich Trott
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

function checkArgumentAlignment(context, node) {

  function isNodeFirstInLine(node, byEndLocation) {
    const firstToken = byEndLocation === true ? context.getLastToken(node, 1) :
      context.getTokenBefore(node);
    const startLine = byEndLocation === true ? node.loc.end.line :
      node.loc.start.line;
    const endLine = firstToken ? firstToken.loc.end.line : -1;

    return startLine !== endLine;
  }

  if (node.arguments.length === 0)
    return;

  var msg = '';
  const first = node.arguments[0];
  var currentLine = first.loc.start.line;
  const firstColumn = first.loc.start.column;

  const ignoreTypes = [
    'ArrowFunctionExpression',
    'FunctionExpression',
    'ObjectExpression',
  ];

  const args = node.arguments;

  // For now, don't bother trying to validate potentially complicating things
  // like closures. Different people will have very different ideas and it's
  // probably best to implement configuration options.
  if (args.some((node) => { return ignoreTypes.indexOf(node.type) !== -1; })) {
    return;
  }

  if (!isNodeFirstInLine(node)) {
    return;
  }

  var misaligned;

  args.slice(1).forEach((argument) => {
    if (!misaligned) {
      if (argument.loc.start.line === currentLine + 1) {
        if (argument.loc.start.column !== firstColumn) {
          if (isNodeFirstInLine(argument)) {
            msg = 'Function argument in column ' +
                  `${argument.loc.start.column + 1}, ` +
                  `expected in ${firstColumn + 1}`;
            misaligned = argument;
          }
        }
      }
    }
    currentLine = argument.loc.start.line;
  });

  if (msg)
    context.report(misaligned, msg);
}

module.exports = function(context) {
  return {
    'CallExpression': (node) => checkArgumentAlignment(context, node)
  };
};
