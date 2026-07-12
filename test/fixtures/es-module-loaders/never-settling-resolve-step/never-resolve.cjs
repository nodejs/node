'use strict';

const neverSettlingDynamicImport = import('never-settle-resolve');

console.log('should be output');

neverSettlingDynamicImport.then(() => process.exit(1));
