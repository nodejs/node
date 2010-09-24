var binding = require('./build/default/binding');

function js() {
  return (new Date()).getTime();
}

var cxx = binding.hello;

var i, N = 10000000;

console.log(js());
console.log(cxx());



var start = new Date();
for (i = 0; i < N; i++) {
  js();
}
var jsDiff = new Date() - start;
console.log(N +" JS function calls: " + jsDiff);


var start = new Date();
for (i = 0; i < N; i++) {
  cxx();
}
var cxxDiff = new Date() - start;
console.log(N +" C++ function calls: " + cxxDiff);

console.log("\nJS speedup " + (cxxDiff / jsDiff)); 


