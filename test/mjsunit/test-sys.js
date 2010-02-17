process.mixin(require("./common"));
process.mixin(require("sys"));

assert.equal("0", inspect(0));
assert.equal("1", inspect(1));
assert.equal("false", inspect(false));
assert.equal('""', inspect(""));
assert.equal('"hello"', inspect("hello"));
assert.equal("[Function]", inspect(function() {}));
assert.equal('undefined', inspect(undefined));
assert.equal('null', inspect(null));
assert.equal('/foo(bar\\n)?/gi', inspect(/foo(bar\n)?/gi));
assert.equal('Sun, 14 Feb 2010 11:48:40 GMT',
        inspect(new Date("Sun, 14 Feb 2010 11:48:40 GMT")));

assert.equal("\"\\n\\u0001\"", inspect("\n\u0001"));

assert.equal('[]', inspect([]));
assert.equal('[]', inspect(Object.create([])));
assert.equal('[\n 1,\n 2\n]', inspect([1, 2]));
assert.equal('[\n 1,\n [\n  2,\n  3\n ]\n]', inspect([1, [2, 3]]));

assert.equal('{}', inspect({}));
assert.equal('{\n "a": 1\n}', inspect({a: 1}));
assert.equal('{\n "a": [Function]\n}', inspect({a: function() {}}));
assert.equal('{\n "a": 1,\n "b": 2\n}', inspect({a: 1, b: 2}));
assert.equal('{\n "a": {}\n}', inspect({'a': {}}));
assert.equal('{\n "a": {\n  "b": 2\n }\n}', inspect({'a': {'b': 2}}));
assert.equal('[\n 1,\n 2,\n 3,\n [length]: 3\n]', inspect([1,2,3], true));
assert.equal("{\n \"visible\": 1\n}",
  inspect(Object.create({}, {visible:{value:1,enumerable:true},hidden:{value:2}}))
);
assert.equal("{\n [hidden]: 2,\n \"visible\": 1\n}",
  inspect(Object.create({}, {visible:{value:1,enumerable:true},hidden:{value:2}}), true)
);

// Objects without prototype
assert.equal(
  "{\n [hidden]: \"secret\",\n \"name\": \"Tim\"\n}",
  inspect(Object.create(null, {name: {value: "Tim", enumerable: true}, hidden: {value: "secret"}}), true)
);
assert.equal(
  "{\n \"name\": \"Tim\"\n}",
  inspect(Object.create(null, {name: {value: "Tim", enumerable: true}, hidden: {value: "secret"}}))
);


// Dynamic properties
assert.equal(
  "{\n \"readonly\": [Getter],\n \"readwrite\": [Getter/Setter],\n \"writeonly\": [Setter]\n}",
  inspect({get readonly(){},get readwrite(){},set readwrite(val){},set writeonly(val){}})
);

var value = {};
value['a'] = value;
assert.equal('{\n "a": [Circular]\n}', inspect(value));
value = Object.create([]);
value.push(1);
assert.equal("[\n 1,\n \"length\": 1\n]", inspect(value));

// Array with dynamic properties
value = [1,2,3];
value.__defineGetter__('growingLength', function () { this.push(true); return this.length; });
assert.equal(
  "[\n 1,\n 2,\n 3,\n \"growingLength\": [Getter]\n]",
  inspect(value)
);

// Function with properties
value = function () {};
value.aprop = 42;
assert.equal(
  "{ [Function]\n \"aprop\": 42\n}",
  inspect(value)
);

// Regular expressions with properties
value = /123/ig;
value.aprop = 42;
assert.equal(
  "{ /123/gi\n \"aprop\": 42\n}",
  inspect(value)
);

// Dates with properties
value = new Date("Sun, 14 Feb 2010 11:48:40 GMT");
value.aprop = 42;
assert.equal(
  "{ Sun, 14 Feb 2010 11:48:40 GMT\n \"aprop\": 42\n}",
  inspect(value)
);
