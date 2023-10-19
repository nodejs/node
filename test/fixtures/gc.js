let arr = new Array(300_000).fill('a');

for (let index = 0; index < arr.length; index++) {
  arr[index] = Math.random();
}

arr = [];
// .gc() is called to generate a Mark-sweep event
global.gc();
