var nodeuuid = require('../uuid'),
    uuidjs = require('uuid').generate,
    N = 5e5;

function rate(msg, t) {
  console.log(msg + ': ' +
    (N / (Date.now() - t) * 1e3 | 0) +
    ' uuids/second');
}

// node-uuid - string form
for (var i = 0, t = Date.now(); i < N; i++) nodeuuid();
rate('nodeuuid()', t);

for (var i = 0, t = Date.now(); i < N; i++) nodeuuid('binary');
rate('nodeuuid(\'binary\')', t);

var buffer = new nodeuuid.BufferClass(16);
for (var i = 0, t = Date.now(); i < N; i++) nodeuuid('binary', buffer);
rate('nodeuuid(\'binary\', buffer)', t);

// node-uuid - string form
for (var i = 0, t = Date.now(); i < N; i++) uuidjs();
rate('uuidjs()', t);

for (var i = 0, t = Date.now(); i < N; i++) uuidjs('binary');
rate('uuidjs(\'binary\')', t);
