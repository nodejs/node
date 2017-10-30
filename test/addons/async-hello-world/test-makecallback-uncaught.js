'use strict';
const common = require('../../common');
const { runMakeCallback } = require(`./build/${common.buildType}/binding`);

process.on('uncaughtException', common.mustCall());

runMakeCallback(5, common.mustCall(() => {
  throw new Error('foo');
}));
