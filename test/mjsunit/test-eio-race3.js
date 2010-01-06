process.mixin(require("./common"));

puts('first stat ...');

posix.stat(__filename)
  .addCallback( function(stats) {
    puts('second stat ...');
    posix.stat(__filename)
      .timeout(1000)
      .wait();

    puts('test passed');
  })
  .addErrback(function() {
    throw new Exception();
  });