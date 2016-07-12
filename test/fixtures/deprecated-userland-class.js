const util = require('util');
const assert = require('assert');

class deprecatedClass {
}

const deprecated = util.deprecate(deprecatedClass, 'deprecatedClass is deprecated.');

const instance = new deprecated();

assert(instance instanceof deprecated);
assert(instance instanceof deprecatedClass);
