var types = 'Array Buffer Int8Array Uint8Array Int16Array Uint16Array Int32Array Uint32Array Float32Array Float64Array'.split(' ');

var type = types[types.indexOf(process.argv[2])];
if (!type)
  type = types[0];

console.error('Benchmarking', type);
var clazz = global[type];

var arr = new clazz(25 * 10e5);
for (var i = 0; i < 10; ++i) {
  for (var j = 0, k = arr.length; j < k; ++j) {
    arr[j] = 0;
  }
}
