var exec = require('child_process').exec, 
    puts = require('sys').puts;

[0, 1].forEach(function(i) {
 exec('ls', function(err, stdout, stderr) {
   puts(i);
   throw new Error('hello world');
 });
});
