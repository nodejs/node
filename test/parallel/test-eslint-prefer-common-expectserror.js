'use strict';

require('../common');

const RuleTester = require('../../tools/node_modules/eslint').RuleTester;
const rule = require('../../tools/eslint-rules/prefer-common-expectserror');

const message = 'Please use common.expectsError(fn, err) instead of ' +
                'assert.throws(fn, common.expectsError(err)).';

new RuleTester().run('prefer-common-expectserror', rule, {
  valid: [
    'assert.throws(fn, /[a-z]/)',
    'assert.throws(function () {}, function() {})',
    'common.expectsError(function() {}, err)'
  ],
  invalid: [
    {
      code: 'assert.throws(function() {}, common.expectsError(err))',
      errors: [{ message }]
    },
    {
      code: 'assert.throws(fn, common.expectsError(err))',
      errors: [{ message }]
    }
  ]
});
