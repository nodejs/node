'use strict';

const common = require('../common');


const ArrayIteratorPrototype =
    Object.getPrototypeOf(Array.prototype[Symbol.iterator]());

delete Array.prototype[Symbol.iterator];
delete ArrayIteratorPrototype.next;

require('../common/fixtures');
import('../fixtures/es-modules/test-esm-ok.mjs').then(common.mustCall());
