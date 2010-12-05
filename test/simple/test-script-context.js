var common = require('../common');
var assert = require('assert');

var Script = require('vm').Script;
var script = new Script('"passed";');

common.debug('run in a new empty context');
var context = script.createContext();
var result = script.runInContext(context);
assert.equal('passed', result);

common.debug('create a new pre-populated context');
context = script.createContext({'foo': 'bar', 'thing': 'lala'});
assert.equal('bar', context.foo);
assert.equal('lala', context.thing);

common.debug('test updating context');
script = new Script('foo = 3;');
result = script.runInContext(context);
assert.equal(3, context.foo);
assert.equal('lala', context.thing);


// Issue GH-227:
Script.runInNewContext('', null, 'some.js');
