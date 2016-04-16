var convert = require('./convert'),
    func = convert('property', require('../property'), require('./_falseOptions'));

func.placeholder = require('./placeholder');
module.exports = func;
