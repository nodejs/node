'use strict';

global.counter ??= 0;
global.counter++;

require('./recursive-b.cjs');
