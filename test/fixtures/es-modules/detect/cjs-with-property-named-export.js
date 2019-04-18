// See ./cjs-with-property-named-import.js

global.export = 3;

global['export'] = 3;

const obj = {
export: 3 // Specifically at column 0, to try to trick the detector
}

console.log(require('process').version);
