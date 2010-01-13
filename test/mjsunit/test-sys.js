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
assert.equal('{\n "0": 1,\n "length": 1\n}', inspect(value));

// Array with dynamic properties
value = [1,2,3];
value.__defineGetter__('growingLength', function () { this.push(true); return this.length; });
assert.equal(
  "{\n \"0\": 1,\n \"1\": 2,\n \"2\": 3,\n \"growingLength\": [Getter]\n}",
  inspect(value)
);