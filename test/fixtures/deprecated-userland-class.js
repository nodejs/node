const util = require('util');
const assert = require('assert');

class deprecatedClass {
}

const deprecated = util.deprecate(deprecatedClass, 'deprecatedClass is deprecated.');

const instance = new deprecated();
const deprecatedInstance = new deprecatedClass();

assert(instance instanceof deprecated);
assert(instance instanceof deprecatedClass);
assert(deprecatedInstance instanceof deprecated);
assert(deprecatedInstance instanceof deprecatedClass);
