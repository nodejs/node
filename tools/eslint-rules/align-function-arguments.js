/**
 * @fileoverview Align arguments in multiline function calls
 * @author Rich Trott
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------
function checkArgumentAlignment(context, node) {
  if (node.arguments.length === 0)
    return;

  var msg = '';
  const first = node.arguments[0];
  var currentLine = first.loc.start.line;
  const firstColumn = first.loc.start.column;

  node.arguments.slice(1).forEach((argument) => {
    if (argument.loc.start.line > currentLine) {
      // Ignore if there are multiple lines between arguments. This happens
      // in situations like this:
      //    setTimeout(function() {
      //      ... do stuff ...
      //    }, 1);
      if (argument.loc.start.line === currentLine + 1) {
        if (argument.loc.start.column !== firstColumn) {
          msg = 'Function called with argument in column ' +
                `${argument.loc.start.column}, expected in ${firstColumn}`;
        }
      }
      currentLine = argument.loc.start.line;
    }
  });

  if (msg)
    context.report(node, msg);
}

module.exports = function(context) {
  return {
    'CallExpression': (node) => checkArgumentAlignment(context, node)
  };
};
