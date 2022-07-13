'use strict';
// Flags: --no-force-node-api-uncaught-exceptions-policy

const common = require('../../common');
const binding = require(`./build/${common.buildType}/binding`);

process.on(
  'uncaughtException',
  common.mustNotCall('uncaught callback errors should be suppressed ' +
    'with the option --no-force-node-api-uncaught-exceptions-policy')
);

binding.CallIntoModule(
  common.mustCall(() => {
    throw new Error('callback error');
  }),
  {},
  'resource_name',
  common.mustCall(function finalizer() {
    throw new Error('finalizer error');
  })
);
