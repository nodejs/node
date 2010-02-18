process.mixin(require("./common"));

var target = function() {};
process.mixin(target, {
  foo: 'bar'
});

assert.equal('bar', target.foo);

// This test verifies there are no DOM-related aspects to process.mixin which
// originally had been in there due to its jQuery origin.
var fakeDomElement = {deep: {nodeType: 4}};
target = {};
process.mixin(true, target, fakeDomElement);

assert.notStrictEqual(target.deep, fakeDomElement.deep);

var objectWithUndefinedValue = {foo: undefined};
target = {};

process.mixin(target, objectWithUndefinedValue);
assert.ok(target.hasOwnProperty('foo'));

// This test verifies getters and setters being copied correctly

var source = {
  _foo:'a',
  get foo(){ return this._foo; },
  set foo(value){ this._foo = "did set to "+value; }
};
var target = {};
process.mixin(target, source);
target._foo = 'b';
assert.equal(source.foo, 'a');
assert.equal('b', target.foo, 'target.foo != "b" -- value/result was copied instead of getter function');
source.foo = 'c';
assert.equal('did set to c', source.foo, 'source.foo != "c" -- value was set instead of calling setter function');
