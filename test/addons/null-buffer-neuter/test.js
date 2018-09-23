'use strict';
// Flags: --expose-gc

require('../../common');
var binding = require('./build/Release/binding');

binding.run();
