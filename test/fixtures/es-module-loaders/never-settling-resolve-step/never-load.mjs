const neverSettlingDynamicImport = import('never-settle-load');

console.log('should be output');

await neverSettlingDynamicImport;
