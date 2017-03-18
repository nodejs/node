'use strict';

const common = require('../common.js');

const obj = {
  array_push______: function(n){
    var arr = [];
    for(let i=0; i<=n; i++) arr.push(i);

    return arr;
  },

  array_unshift___: function(n){
    var arr = [];
    for(let i=n; i--;) arr.unshift(i);

    return arr;
  },

  array_index_incr: function(n){
    var arr = new Array(n);
    for(let i=0; i<=n; i++) arr[i] = i;

    return arr;
  },

  array_index_decr: function(n){
    var arr = new Array(n);
    for(let i=n; i--;) arr[i] = i;

    return arr;
  },
};

const bench = common.createBenchmark(main, {
  type: Object.keys(obj),
  n: [1000],
});


function main(conf) {
  'use strict';
  const
    func = obj[conf.type],
    n    = +conf.n;

  bench.start();
  func(n);
  // console.log(String(func));
  bench.end(n);
}