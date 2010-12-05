var common = require('../common');
var assert = require('assert');

var Script = require('vm').Script;

common.debug('run in a new empty context');
var context = Script.createContext();
var result = Script.runInContext('"passed";', context);
assert.equal('passed', result);

common.debug('create a new pre-populated context');
context = Script.createContext({'foo': 'bar', 'thing': 'lala'});
assert.equal('bar', context.foo);
assert.equal('lala', context.thing);

common.debug('test updating context');
result = Script.runInContext('var foo = 3;', context);
assert.equal(3, context.foo);
assert.equal('lala', context.thing);
