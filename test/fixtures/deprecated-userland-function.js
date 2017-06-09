const util = require('util');

function deprecatedFunction() {
}

util.deprecate(deprecatedFunction, 'deprecatedFunction is deprecated.')();
