self.addEventListener('message', function(e) {
    var message = e.data;
    if (message.action === 'create') {
      e.waitUntil(deleteDB()
          .then(doIndexedDBTest)
          .then(function() {
              message.port.postMessage({ type: 'created' });
            })
          .catch(function(reason) {
              message.port.postMessage({ type: 'error', value: reason });
            }));
    } else if (message.action === 'cleanup') {
      e.waitUntil(deleteDB()
          .then(function() {
              message.port.postMessage({ type: 'done' });
            })
          .catch(function(reason) {
              message.port.postMessage({ type: 'error', value: reason });
            }));
    }
  });

function deleteDB() {
  return new Promise(function(resolve, reject) {
      var delete_request = indexedDB.deleteDatabase('db');

      delete_request.onsuccess = resolve;
      delete_request.onerror = reject;
    });
}

function doIndexedDBTest(port) {
  return new Promise(function(resolve, reject) {
      var open_request = indexedDB.open('db');

      open_request.onerror = reject;
      open_request.onupgradeneeded = function() {
        var db = open_request.result;
        db.createObjectStore('store');
      };
      open_request.onsuccess = function() {
        var db = open_request.result;
        var tx = db.transaction('store', 'readwrite');
        var store = tx.objectStore('store');
        store.put('value', 'key');

        tx.onerror = function() {
            db.close();
            reject(tx.error);
          };
        tx.oncomplete = function() {
            db.close();
            resolve();
          };
      };
    });
}
