let x = 0;
function heartbeat() {
  ++x;
}
setInterval(heartbeat, 50);

if (process.send) process.send('ready');
