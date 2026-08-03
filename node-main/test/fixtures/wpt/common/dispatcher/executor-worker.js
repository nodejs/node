importScripts('./dispatcher.js');

const params = new URLSearchParams(location.search);
const uuid = params.get('uuid');

let executeOrders = async function() {
  while(true) {
    let task = await receive(uuid);
    eval(`(async () => {${task}})()`);
  }
};
executeOrders();
