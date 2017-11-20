'use strict';
const BUFFER_REQUIRE = 'const { Buffer } = require(\'buffer\');\n';

module.exports = function(context) {

  function flagIt(reference) {
    const msg = 'Use const Buffer = require(\'buffer\').Buffer; ' +
                'at the beginning of this file';

    context.report({
      node: reference.identifier,
      message: msg,
      fix: (fixer) => {
        const sourceCode = context.getSourceCode();

        const useStrict = /'use strict';\n\n?/g;
        const hasUseStrict = !!useStrict.exec(sourceCode.text);
        const firstLOC = sourceCode.ast.range[0];
        const rangeNeedle = hasUseStrict ? useStrict.lastIndex : firstLOC;

        return fixer.insertTextBeforeRange([rangeNeedle], BUFFER_REQUIRE);
      }
    });
  }

  return {
    'Program:exit': function() {
      const globalScope = context.getScope();
      const variable = globalScope.set.get('Buffer');
      if (variable) {
        variable.references.forEach(flagIt);
      }
    }
  };
};
