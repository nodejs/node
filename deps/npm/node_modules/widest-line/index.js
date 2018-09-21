'use strict';
const stringWidth = require('string-width');

module.exports = input => Math.max.apply(null, input.split('\n').map(x => stringWidth(x)));
