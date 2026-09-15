addEventListener('message', function(evt) {
  if (evt.data.type === 'GET_CLIENTS') {
    clients.matchAll(evt.data.opts).then(function(clientList) {
      var resultList = clientList.map(function(c) {
        return { url: c.url, frameType: c.frameType, id: c.id };
      });
      evt.source.postMessage({ type: 'success', detail: resultList });
    }).catch(function(err) {
      evt.source.postMessage({
        type: 'failure',
        detail: 'matchAll() rejected with "' + err + '"'
      });
    });
    return;
  }

  evt.source.postMessage({
    type: 'failure',
    detail: 'Unexpected message type "' + evt.data.type + '"'
  });
});
