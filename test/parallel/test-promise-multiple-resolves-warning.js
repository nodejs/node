'use strict';

const common = require('../common');

const rejection = new Error('Swallowed reject');
const resolveMessage = 'First call';
const swallowedResolve = 'Swallowed resolve';

new Promise((resolve, reject) => {
  resolve(resolveMessage);
  resolve(swallowedResolve);
  reject(rejection);
}).then(common.mustCall());

common.expectWarning(
  'MultiplePromiseResolvesWarning',
  'A promise constructor called `resolve()` or `reject()` more than ' +
    'once or both function together. It is recommended to use ' +
    "`process.on('multipleResolves')` to detect this early on and to " +
    'always only call one of these functions per execution.',
  common.noWarnCode
);
