const result = await Promise.race([
    import('never-settle-resolve'),
    import('never-settle-load'),
    import('node:process'),
]);

await import('data:text/javascript,');

console.log(result.default === process);
