const exec = require('child_process').exec;

[0, 1].forEach((i) => {
  exec('ls', (err, stdout, stderr) => {
    console.log(i);
    throw new Error('hello world');
  });
});
