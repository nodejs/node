'use strict';

require('../common');
const { inspect } = require('node:util');

const r = Proxy.revocable({}, {});
r.revoke();

console.dir(r);
console.dir(r.proxy);
console.log(r.proxy);
console.log(inspect(r.proxy, { showProxy: true }));
