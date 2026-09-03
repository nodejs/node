// META: global=window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: script=cache-storage/resources/test-helpers.js
// META: script=service-worker/resources/test-helpers.sub.js
// META: timeout=long

// https://w3c.github.io/ServiceWorker

idl_test(
  ['service-workers'],
  ['dom', 'html'],
  async (idl_array, t) => {
    self.cacheInstance = await create_temporary_cache(t);

    idl_array.add_objects({
      CacheStorage: ['caches'],
      Cache: ['self.cacheInstance'],
      ServiceWorkerContainer: ['navigator.serviceWorker']
    });

    // TODO: Add ServiceWorker and ServiceWorkerRegistration instances for the
    // other worker scopes.
    if (self.GLOBAL.isWindow()) {
      idl_array.add_objects({
        ServiceWorkerRegistration: ['registrationInstance'],
        ServiceWorker: ['registrationInstance.installing']
      });

      const scope = 'service-worker/resources/scope/idlharness';
      const registration = await service_worker_unregister_and_register(
          t, 'service-worker/resources/empty-worker.js', scope);
      t.add_cleanup(() => registration.unregister());

      self.registrationInstance = registration;
    } else if (self.ServiceWorkerGlobalScope) {
      // self.ServiceWorkerGlobalScope should only be defined for the
      // ServiceWorker scope, which allows us to detect and test the interfaces
      // exposed only for ServiceWorker.
      idl_array.add_objects({
        Clients: ['clients'],
        ExtendableEvent: ['new ExtendableEvent("type")'],
        FetchEvent: ['new FetchEvent("type", { request: new Request("") })'],
        ServiceWorkerGlobalScope: ['self'],
        ServiceWorkerRegistration: ['registration'],
        ServiceWorker: ['serviceWorker'],
        // TODO: Test instances of Client and WindowClient, e.g.
        // Client: ['self.clientInstance'],
        // WindowClient: ['self.windowClientInstance']
      });
    }
  }
);
