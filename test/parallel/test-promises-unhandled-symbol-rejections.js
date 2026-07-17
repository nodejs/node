// Flags: --unhandled-rejections=warn
'use strict';
const common = require('../common');

const expectedValueWarning = ['Symbol()'];
const expectedPromiseWarning = ['Unhandled promise rejection. ' +
  'This error originated either by throwing ' +
  'inside of an async function without a catch ' +
  'block, or by rejecting a promise which was ' +
  'not handled with .catch(). To terminate the ' +
  'node process on unhandled promise rejection, ' +
  'use the CLI flag `--unhandled-rejections=strict` (see ' +
  'https://nodejs.org/api/cli.html#cli_unhandled_rejections_mode). ' +
  '(rejection id: 1)'];

common.expectWarning({
  UnhandledPromiseRejectionWarning: [
    expectedValueWarning,
    expectedPromiseWarning,
  ],
});

// Ensure this doesn't crash
Promise.reject(Symbol());
