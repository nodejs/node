'use strict';
let x = 0;
function heartbeat() {
  ++x;
}
setInterval(heartbeat, 50);
