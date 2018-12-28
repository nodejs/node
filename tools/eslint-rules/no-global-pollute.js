/**
 * @fileOverview This rule makes sure that the globals don't get polluted by
 * adding new entries to `global`.
 * @author Ruben Bridgewater <ruben@bridgewater.de>
 */

'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
  const filename = context.getFilename();

  return {
    'Program:exit': function() {
      const globalScope = context.getScope();
      const variable = globalScope.set.get('global');
      if (variable &&
          !filename.includes('bootstrap') &&
          !filename.includes('repl.js')) {
        const msg = 'Please do not pollute the global scope';
        variable.references.forEach((reference) => {
          context.report({
            node: reference.identifier,
            message: msg
          });
        });
      }
    }
  };
};
