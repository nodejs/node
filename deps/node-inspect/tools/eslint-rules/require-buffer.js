'use strict';

module.exports = function(context) {
  function flagIt(reference) {
    const msg = 'Use const Buffer = require(\'buffer\').Buffer; ' +
                'at the beginning of this file';
    context.report(reference.identifier, msg);
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
