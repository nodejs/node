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