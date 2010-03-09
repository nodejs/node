require("../common");

assert.equal("0", inspect(0));
assert.equal("1", inspect(1));
assert.equal("false", inspect(false));
assert.equal("''", inspect(""));
assert.equal("'hello'", inspect("hello"));
assert.equal("[Function]", inspect(function() {}));
assert.equal('undefined', inspect(undefined));
assert.equal('null', inspect(null));
assert.equal('/foo(bar\\n)?/gi', inspect(/foo(bar\n)?/gi));
assert.equal('Sun, 14 Feb 2010 11:48:40 GMT',
        inspect(new Date("Sun, 14 Feb 2010 11:48:40 GMT")));

assert.equal("'\\n\\u0001'", inspect("\n\u0001"));

assert.equal('[]', inspect([]));
assert.equal('[]', inspect(Object.create([])));
assert.equal('[ 1, 2 ]', inspect([1, 2]));
assert.equal('[ 1, [ 2, 3 ] ]', inspect([1, [2, 3]]));

assert.equal('{}', inspect({}));
assert.equal('{ a: 1 }', inspect({a: 1}));
assert.equal('{ a: [Function] }', inspect({a: function() {}}));
assert.equal('{ a: 1, b: 2 }', inspect({a: 1, b: 2}));
assert.equal('{ a: {} }', inspect({'a': {}}));
assert.equal('{ a: { b: 2 } }', inspect({'a': {'b': 2}}));
assert.equal('{ a: { b: { c: [Object] } } }', inspect({'a': {'b': { 'c': { 'd': 2 }}}}));
assert.equal('{ a: { b: { c: { d: 2 } } } }', inspect({'a': {'b': { 'c': { 'd': 2 }}}}, false, null));
assert.equal('[ 1, 2, 3, [length]: 3 ]', inspect([1,2,3], true));
assert.equal('{ a: [Object] }', inspect({'a': {'b': { 'c': 2}}},false,0));
assert.equal('{ a: { b: [Object] } }', inspect({'a': {'b': { 'c': 2}}},false,1));
assert.equal("{ visible: 1 }",
  inspect(Object.create({}, {visible:{value:1,enumerable:true},hidden:{value:2}}))
);
assert.equal("{ [hidden]: 2, visible: 1 }",
  inspect(Object.create({}, {visible:{value:1,enumerable:true},hidden:{value:2}}), true)
);

// Objects without prototype
assert.equal(
  "{ [hidden]: 'secret', name: 'Tim' }",
  inspect(Object.create(null, {name: {value: "Tim", enumerable: true}, hidden: {value: "secret"}}), true)
);
assert.equal(
  "{ name: 'Tim' }",
  inspect(Object.create(null, {name: {value: "Tim", enumerable: true}, hidden: {value: "secret"}}))
);


// Dynamic properties
assert.equal(
  "{ readonly: [Getter] }",
  inspect({get readonly(){}})
);
assert.equal(
  "{ readwrite: [Getter/Setter] }",
  inspect({get readwrite(){},set readwrite(val){}})
);
assert.equal(
  "{ writeonly: [Setter] }",
  inspect({set writeonly(val){}})
);

var value = {};
value['a'] = value;
assert.equal('{ a: [Circular] }', inspect(value));
value = Object.create([]);
value.push(1);
assert.equal("[ 1, length: 1 ]", inspect(value));

// Array with dynamic properties
value = [1,2,3];
value.__defineGetter__('growingLength', function () { this.push(true); return this.length; });
assert.equal(
  "[ 1, 2, 3, growingLength: [Getter] ]",
  inspect(value)
);

// Function with properties
value = function () {};
value.aprop = 42;
assert.equal(
  "{ [Function] aprop: 42 }",
  inspect(value)
);

// Regular expressions with properties
value = /123/ig;
value.aprop = 42;
assert.equal(
  "{ /123/gi aprop: 42 }",
  inspect(value)
);

// Dates with properties
value = new Date("Sun, 14 Feb 2010 11:48:40 GMT");
value.aprop = 42;
assert.equal(
  "{ Sun, 14 Feb 2010 11:48:40 GMT aprop: 42 }",
  inspect(value)
);
