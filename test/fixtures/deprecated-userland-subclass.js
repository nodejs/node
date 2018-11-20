const util = require('util');
const assert = require('assert');

class deprecatedClass {
}

const deprecated = util.deprecate(deprecatedClass, 'deprecatedClass is deprecated.');

class subclass extends deprecated {
  constructor() {
    super();
  }
}

const instance = new subclass();

assert(instance instanceof subclass);
assert(instance instanceof deprecated);
assert(instance instanceof deprecatedClass);
