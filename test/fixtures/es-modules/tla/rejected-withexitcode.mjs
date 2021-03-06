process.exitCode = 42;
await Promise.reject(new Error('Xyz'));
