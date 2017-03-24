'use strict';

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const watch = fs.watchFile(__filename, common.noop);
watch.once('stop', assert.fail);  // Should not trigger.
watch.stop();
watch.removeListener('stop', assert.fail);
