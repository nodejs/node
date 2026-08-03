const obj = require('./b.js');

console.log('cycle equality', obj.a.b === obj);
