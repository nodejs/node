'use strict';

const common = require('../common');
const { EventEmitter: EE } = require('events');

{
  const e = new EE();
  e.addListener = common.mustCall();
  e.on('test', () => {});
}

{
  const e = new EE();
  e.removeListener = common.mustCall();
  e.off('test', () => {});
}
