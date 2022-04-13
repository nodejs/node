'use strict';
const common = require('../common');
const { once } = require('events');

const et = new EventTarget();
(async function() {
  await once(et, 'foo');
  await once(et, 'foo');
})().then(common.mustCall());

et.dispatchEvent(new Event('foo'));
setImmediate(() => {
  et.dispatchEvent(new Event('foo'));
});
