const result = await Promise.race([
    import('never-settle-resolve'),
    import('never-settle-load'),
    import('node:process'),
]);

console.log(result.default === process);
