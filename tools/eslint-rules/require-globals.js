'use strict';

// This rule makes sure that no Globals are going to be used in /lib.
// That could otherwise result in problems with the repl.

module.exports = function(context) {

  function flagIt(msg, fix) {
    return (reference) => {
      context.report({
        node: reference.identifier,
        message: msg,
        fix: (fixer) => {
          const sourceCode = context.getSourceCode();

          const useStrict = /'use strict';\n\n?/g;
          const hasUseStrict = !!useStrict.exec(sourceCode.text);
          const firstLOC = sourceCode.ast.range[0];
          const rangeNeedle = hasUseStrict ? useStrict.lastIndex : firstLOC;

          return fixer.insertTextBeforeRange([rangeNeedle], `${fix}\n`);
        }
      });
    };
  }

  return {
    'Program:exit': function() {
      const globalScope = context.getScope();
      let variable = globalScope.set.get('Buffer');
      if (variable) {
        const fix = "const { Buffer } = require('buffer');";
        const msg = `Use ${fix} at the beginning of this file`;
        variable.references.forEach(flagIt(msg, fix));
      }
      variable = globalScope.set.get('URL');
      if (variable) {
        const fix = "const { URL } = require('url');";
        const msg = `Use ${fix} at the beginning of this file`;
        variable.references.forEach(flagIt(msg, fix));
      }
      variable = globalScope.set.get('URLSearchParams');
      if (variable) {
        const fix = "const { URLSearchParams } = require('url');";
        const msg = `Use ${fix} at the beginning of this file`;
        variable.references.forEach(flagIt(msg, fix));
      }
    }
  };
};
