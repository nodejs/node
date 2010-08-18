foo = "foo";
global.bar = "bar";

exports.fooBar = function () {
  return {foo: global.foo, bar: bar};
};
