include("mjsunit");

var port = 8222;

function onLoad() {

  var request_number = 0;

  new node.http.Server(function (req, res) {
    res.id = request_number;
    req.id = request_number++;


    if (req.id == 0) {
      //puts("get req");
      assertEquals("GET", req.method);
      assertEquals("/hello", req.uri.path);
    }

    if (req.id == 1) {
      //puts("post req");
      assertEquals("POST", req.method);
      assertEquals("/quit", req.uri.path);
      this.close();
      //puts("server closed");
    }

    setTimeout(function () {
      //puts("send response");
      res.sendHeader(200, [["Content-Type", "text/plain"]]);
      res.sendBody(req.uri.path);
      res.finish();
    }, 1);

  }).listen(port);

  var c = new node.tcp.Connection();
  var req_sent = 0;
  c.onConnect = function () {
    //puts("send get");
    c.send( "GET /hello HTTP/1.1\r\n\r\n" );
    req_sent += 1;
  };
  var total = "";

  c.onReceive = function (chunk) {
    //puts("client recv");
    total += chunk.encodeUtf8();
    puts("total: " + JSON.stringify(total));

    if ( req_sent == 1) {
      puts("send post");
      c.send("POST /quit HTTP/1.1\r\n\r\n");
      c.close();
      puts("client half close");
      assertEquals(c.readyState, "readOnly");
      req_sent += 1;
    }
  };

  c.onEOF = function () {
    puts("client got eof");
  };

  c.onDisconnect = function () {
    puts("client disconnected");

    assertEquals(c.readyState, "closed");

    var hello = new RegExp("/hello");
    assertTrue(hello.exec(total) != null);

    var quit = new RegExp("/quit");
    assertTrue(quit.exec(total) != null);
  };

  c.connect(port);
}
