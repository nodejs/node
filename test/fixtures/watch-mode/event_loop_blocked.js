console.log('running');
Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0);
console.log('don\'t show me');

