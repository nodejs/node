/**
 * @fileoverview Enforce propTypes declarations alphabetical sorting
 * @deprecated
 */
'use strict';

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

var util = require('util');
var sortPropTypes = require('./sort-prop-types');
var isWarnedForDeprecation = false;

module.exports = function(context) {
  return util._extend(sortPropTypes(context), {
    Program: function() {
      if (isWarnedForDeprecation || /\=-(f|-format)=/.test(process.argv.join('='))) {
        return;
      }

      /* eslint-disable no-console */
      console.log('The react/jsx-sort-prop-types rule is deprecated. Please ' +
                  'use the react/sort-prop-types rule instead.');
      /* eslint-enable no-console */
      isWarnedForDeprecation = true;
    }
  });
};

module.exports.schema = [{
  type: 'object',
  properties: {
    requiredFirst: {
      type: 'boolean'
    },
    callbacksLast: {
      type: 'boolean'
    },
    ignoreCase: {
      type: 'boolean'
    }
  },
  additionalProperties: false
}];
