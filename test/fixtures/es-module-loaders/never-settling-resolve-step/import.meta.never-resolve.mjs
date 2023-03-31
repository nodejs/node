console.log('should be output');

await import.meta.resolve('never-settle-resolve');

console.log('should not be output');
