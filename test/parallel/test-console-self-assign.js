'use strict';

require('../common');

// Assigning to itself should not throw.
global.console = global.console; // eslint-disable-line no-self-assign
