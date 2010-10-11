/* 
 * try with
 * curl  -d @/usr/share/dict/words http://localhost:8000/123 
 */

http = require('http');

s = http.Server(function (req, res) {
  console.log(req.headers);

  req.pipe(process.stdout, { end: false });

  req.on('end', function () {
    res.writeHead(200);
    res.write("thanks");
    res.end();
  });
});

s.listen(8000);
