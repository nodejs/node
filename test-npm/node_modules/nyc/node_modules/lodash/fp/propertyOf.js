var convert = require('./convert'),
    func = convert('propertyOf', require('../propertyOf'), require('./_falseOptions'));

func.placeholder = require('./placeholder');
module.exports = func;
