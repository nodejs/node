'use strict';

const common = require('../common');
const { EventEmitter: EE } = require('events');

{
  const e = new EE();
  e.on = common.mustCall();
  e.addListener('test', () => {});
}

{
  const e = new EE();
  e.removeListener = common.mustCall();
  e.off('test', () => {});
}
