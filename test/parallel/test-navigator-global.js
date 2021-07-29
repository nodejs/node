'use strict';
/* eslint-disable no-global-assign */
require('../common');
const { notStrictEqual } = require('assert');

performance = undefined;
notStrictEqual(globalThis.performance, undefined);
