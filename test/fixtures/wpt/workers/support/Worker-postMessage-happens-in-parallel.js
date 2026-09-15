onmessage = (e) => {
  if (e.data == 'init') {
    postMessage(0);
  } else {
    e.data[0] = 1;
  }
}
