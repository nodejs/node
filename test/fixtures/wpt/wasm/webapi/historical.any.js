// META: global=window,worker

promise_test(async t => {
  const db_name = "WebAssembly";
  const obj_store = "store";
  const module_key = "module";

  await new Promise((resolve, reject) => {
    const delete_request = indexedDB.deleteDatabase(db_name);
    delete_request.onsuccess = resolve;
    delete_request.onerror = reject;
  });

  const db = await new Promise((resolve, reject) => {
    const open_request = indexedDB.open(db_name);
    open_request.onupgradeneeded = function() {
      open_request.result.createObjectStore(obj_store);
    };
    open_request.onsuccess = function() {
      resolve(open_request.result);
    };
    open_request.onerror = reject;
  });

  const mod = await WebAssembly.compileStreaming(fetch('../incrementer.wasm'));
  const tx = db.transaction(obj_store, 'readwrite');
  const store = tx.objectStore(obj_store);
  assert_throws_dom("DataCloneError", () => store.put(mod, module_key));
});
