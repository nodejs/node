if (global.maybe)
  module.exports = require('../is-object');
exports['invalid identifier'] = 'yes';
module.exports['?invalid'] = 'yes';
module.exports['π'] = 'yes';
exports['\u{D83C}'] = 'no';
exports['\u{D83C}\u{DF10}'] = 'yes';
exports.package  = 10; // reserved word
Object.defineProperty(exports, 'z', { value: 'yes' });
