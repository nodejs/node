process.on('exit', (exitCode) => {
    console.log(`the exit listener received code: ${exitCode}`);
});

process.exitCode = 42;

await new Promise(() => {});
