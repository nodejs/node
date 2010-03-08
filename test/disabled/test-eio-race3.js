/* XXX Can this test be modified to not call the now-removed wait()? */

require("../common");


puts('first stat ...');

fs.stat(__filename)
  .addCallback( function(stats) {
    puts('second stat ...');
    fs.stat(__filename)
      .timeout(1000)
      .wait();

    puts('test passed');
  })
  .addErrback(function() {
    throw new Exception();
  });
