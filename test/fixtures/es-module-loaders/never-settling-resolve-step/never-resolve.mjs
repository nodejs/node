const neverSettlingDynamicImport = import('never-settle-resolve');

console.log('should be output');

await neverSettlingDynamicImport;
