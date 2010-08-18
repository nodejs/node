foo2 = "foo2";
global.bar2 = "bar2";


exports.globalKeys = function () {
  return Object.getOwnPropertyNames(global);
};

exports.allFooBars = function () {
  return {
    foo0: global.foo0,
    bar0: bar0,
    foo1: global.foo1,
    bar1: bar1,
    foo2: global.foo2,
    bar2: bar2
  };
};
