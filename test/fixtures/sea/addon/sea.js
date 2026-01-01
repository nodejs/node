const sea = require('node:sea');
const fs = require('fs');
const path = require('path');

const addonPath = path.join(process.cwd(), 'hello.node');
fs.writeFileSync(addonPath, new Uint8Array(sea.getRawAsset('hello.node')));
const mod = {exports: {}};
process.dlopen(mod, addonPath);
console.log('hello,', mod.exports.hello());
