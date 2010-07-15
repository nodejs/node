common = require("../common");
assert = common.assert

var Script = process.binding('evals').Script;
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
