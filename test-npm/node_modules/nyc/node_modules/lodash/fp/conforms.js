var convert = require('./convert'),
    func = convert('conforms', require('../conforms'), require('./_falseOptions'));

func.placeholder = require('./placeholder');
module.exports = func;
