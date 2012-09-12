var bar = require('./bar');

module.exports = function (x) {
    return x * bar.coeff(x) + (x * 3 - 2)
};
