client = new TCPClient("google.com", 80);

log("readyState: " + client.readyState);
//assertEqual(client.readystate, TCP.CONNECTING);

client.onopen = function () {
  log("connected to google");
  log("readyState: " + client.readyState);
  client.write("GET / HTTP/1.1\r\n\r\n");
};

client.onread = function (chunk) {
  log(chunk);
};

client.onclose = function () {
  log("connection closed");
};



setTimeout(function () {
  client.disconnect();
}, 10 * 1000);


log("hello");
