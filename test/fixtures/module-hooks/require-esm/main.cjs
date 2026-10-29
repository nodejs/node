const { esmValue, cjsValue } = require('./esm.mjs');
console.log('esmValue in main.cjs:', esmValue);
console.log('cjsValue in main.cjs:', cjsValue);
module.exports = { esmValue, cjsValue };
