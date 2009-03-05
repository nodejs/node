/*
[Constructor(in DOMString url)]
interface TCP.Client {
  readonly attribute DOMString host;
  readonly attribute DOMString port;

  // ready state
  const unsigned short CONNECTING = 0;
  const unsigned short OPEN = 1;
  const unsigned short CLOSED = 2;
  readonly attribute long readyState;

  // networking
           attribute Function onopen;
           attribute Function onrecv;
           attribute Function onclose;
  void send(in DOMString data);
  void disconnect();
};
*/

client = new TCPClient("localhost", 11222);

log("readyState: " + client.readyState);
//assertEqual(client.readystate, TCP.CONNECTING);

client.onopen = function () {
  log("connected to dynomite");
  log("readyState: " + client.readyState);
  client.write("get 1 /\n");
};

client.onread = function (chunk) {
  log(chunk);
};

client.onclose = function () {
  log("connection closed");
};

setTimeout(function () {
  client.disconnect();
}, 1000);


