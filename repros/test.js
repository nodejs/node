const fs =require('fs');

const p = fs.realpathSync("/dev/stdin");

console.log(fs.statSync(p));
console.log(fs.statSync("/dev/stdin"));
console.log(p)