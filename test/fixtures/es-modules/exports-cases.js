if (global.maybe)
  module.exports = require('../is-object');
exports['invalid identifier'] = 'yes';
module.exports['?invalid'] = 'yes';
module.exports['π'] = 'yes';
exports.package  = 10; // reserved word
Object.defineProperty(exports, 'z', { value: 'yes' });
