// testharness file with ShadowRealm utilities to be imported inside ShadowRealm

/**
 * Set up all properties on the ShadowRealm's global object that tests will
 * expect to be present.
 *
 * @param {string} queryString - string to use as value for location.search,
 *   used for subsetting some tests
 * @param {function} fetchAdaptor - a function that takes a resource URI and
 *   returns a function which itself takes a (resolve, reject) pair from the
 *   hosting realm, and calls resolve with the text result of fetching the
 *   resource, or reject with a string indicating the error that occurred
 */
globalThis.setShadowRealmGlobalProperties = function (queryString, fetchAdaptor) {
  globalThis.fetch_json = (resource) => {
    const executor = fetchAdaptor(resource);
    return new Promise(executor).then((s) => JSON.parse(s));
  };

  // Used only by idlharness.js
  globalThis.fetch_spec = (spec) => {
    const resource = `/interfaces/${spec}.idl`;
    const executor = fetchAdaptor(resource);
    return new Promise(executor).then(
      idl => ({ spec, idl }),
      () => {
        throw new IdlHarnessError(`Error fetching ${resource}.`);
      });
  }

  globalThis.location = { search: queryString };
};

globalThis.GLOBAL = {
  isWindow: function() { return false; },
  isWorker: function() { return false; },
  isShadowRealm: function() { return true; },
};
