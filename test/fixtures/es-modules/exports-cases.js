if (global.maybe)
  module.exports = require('../is-object');
exports['invalid identifier'] = 'no';
module.exports['?invalid'] = 'no';
module.exports['π'] = 'yes';
exports.package  = 10; // reserved word -> not used
Object.defineProperty(exports, 'z', { value: 'yes' });
