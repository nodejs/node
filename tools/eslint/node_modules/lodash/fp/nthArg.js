var convert = require('./convert'),
    func = convert('nthArg', require('../nthArg'), require('./_falseOptions'));

func.placeholder = require('./placeholder');
module.exports = func;
