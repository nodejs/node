function run_test(data, sender) {
  if (data === 'xhr') {
    const xhr = new XMLHttpRequest();
    xhr.open('GET', 'synthesized-response.txt', true);
    xhr.responseType = 'text';
    xhr.send();
    xhr.onload = evt => sender.postMessage(xhr.responseText);
    xhr.onerror = () => sender.postMessage('XHR failed!');
  } else if (data === 'fetch') {
    fetch('synthesized-response.txt')
        .then(response => response.text())
        .then(data => sender.postMessage(data))
        .catch(error => sender.postMessage('Fetch failed!'));
  } else if (data === 'importScripts') {
    importScripts('synthesized-response.js');
    // |message| is provided by 'synthesized-response.js';
    sender.postMessage(message);
  } else {
    sender.postMessage('Unexpected message! ' + data);
  }
}

// Entry point for dedicated workers.
self.onmessage = evt => run_test(evt.data, self);

// Entry point for shared workers.
self.onconnect = evt => {
  evt.ports[0].onmessage = e => run_test(e.data, evt.ports[0]);
};
