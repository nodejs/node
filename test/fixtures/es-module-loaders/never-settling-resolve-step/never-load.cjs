'use strict';

const neverSettlingDynamicImport = import('never-settle-load');

console.log('should be output');

neverSettlingDynamicImport.then(() => process.exit(1));
