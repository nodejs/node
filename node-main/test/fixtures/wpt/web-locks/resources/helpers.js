// Test helpers used by multiple Web Locks API tests.
(() => {

  // Generate a unique resource identifier, using the script path and
  // test case name. This is useful to avoid lock interference between
  // test cases.
  let res_num = 0;
  self.uniqueName = (testCase, prefix) => {
    return `${self.location.pathname}-${prefix}-${testCase.name}-${++res_num}`;
  };
  self.uniqueNameByQuery = () => {
    const prefix = new URL(location.href).searchParams.get('prefix');
    return `${prefix}-${++res_num}`;
  }

  // Inject an iframe showing the given url into the page, and resolve
  // the returned promise when the frame is loaded.
  self.iframe = url => new Promise(resolve => {
    const element = document.createElement('iframe');
    element.addEventListener(
      'load', () => { resolve(element); }, { once: true });
    element.src = url;
    document.documentElement.appendChild(element);
  });

  // Post a message to the target frame, and resolve the returned
  // promise when a response comes back. The posted data is annotated
  // with unique id to track the response. This assumes the use of
  // 'iframe.html' as the frame, which implements this protocol.
  let next_request_id = 0;
  self.postToFrameAndWait = (frame, data) => {
    const iframe_window = frame.contentWindow;
    data.rqid = next_request_id++;
    iframe_window.postMessage(data, '*');
    return new Promise(resolve => {
      const listener = event => {
        if (event.source !== iframe_window || event.data.rqid !== data.rqid)
          return;
        self.removeEventListener('message', listener);
        resolve(event.data);
      };
      self.addEventListener('message', listener);
    });
  };

  // Post a message to the target worker, and resolve the returned
  // promise when a response comes back. The posted data is annotated
  // with unique id to track the response. This assumes the use of
  // 'worker.js' as the worker, which implements this protocol.
  self.postToWorkerAndWait = (worker, data) => {
    return new Promise(resolve => {
      data.rqid = next_request_id++;
      worker.postMessage(data);
      const listener = event => {
        if (event.data.rqid !== data.rqid)
          return;
        worker.removeEventListener('message', listener);
        resolve(event.data);
      };
      worker.addEventListener('message', listener);
    });
  };

  /**
   * Request a lock and hold it until the subtest ends.
   * @param {*} t test runner object
   * @param {string} name lock name
   * @param {LockOptions=} options lock options
   * @returns
   */
  self.requestLockAndHold = (t, name, options = {}) => {
    let [promise, resolve] = self.makePromiseAndResolveFunc();
    const released = navigator.locks.request(name, options, () => promise);
    // Add a cleanup function that releases the lock by resolving the promise,
    // and then waits until the lock is really released, to avoid contaminating
    // following tests with temporarily held locks.
    t.add_cleanup(() => {
      resolve();
      // Cleanup shouldn't fail if the request is aborted.
      return released.catch(() => undefined);
    });
    return released;
  };

  self.makePromiseAndResolveFunc = () => {
    let resolve;
    const promise = new Promise(r => { resolve = r; });
    return [promise, resolve];
  };

})();
