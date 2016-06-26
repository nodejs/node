'use strict';

require('../common');
const fs = require('fs');
const assert = require('assert');

Object.getOwnPropertyNames(fs).filter(
  (d) => !d.endsWith('Stream') &&       // ignore stream creation functions
         !d.endsWith('Sync') &&         // ignore synchronous functions
         typeof fs[d] === 'function' && // ignore anything other than functions
         d.indexOf('_') === -1 &&       // ignore internal use functions
         !/^[A-Z]/.test(d) &&           // ignore conventional constructors
         d !== 'watch' &&               // watch's callback is optional
         d !== 'unwatchFile'            // unwatchFile's callback is optional
).forEach(
  (d) => assert.throws(() => fs[d](), /"callback" argument must be a function/)
);
