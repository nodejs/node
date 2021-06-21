'use strict';
// Flags: --force-node-api-uncaught-exceptions-policy=false

const common = require('../../common');
const binding = require(`./build/${common.buildType}/binding`);

process.on(
  'uncaughtException',
  common.mustNotCall('uncaught callback errors should be suppressed ' +
    'when the option --force-node-api-uncaught-exceptions-policy=false')
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
