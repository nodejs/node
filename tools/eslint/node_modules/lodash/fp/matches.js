var convert = require('./convert'),
    func = convert('matches', require('../matches'), require('./_falseOptions'));

func.placeholder = require('./placeholder');
module.exports = func;
