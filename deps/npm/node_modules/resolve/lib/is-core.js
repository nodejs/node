var core = require('./core');

module.exports = function isCore(x) {
    return Object.prototype.hasOwnProperty.call(core, x);
};
