interface Foo {
  Promise<Promise<sequence<DOMString?>>> bar();
  readonly attribute Promise<DOMString> baz;
};

// Extracted from https://slightlyoff.github.io/ServiceWorker/spec/service_worker/ on 2014-05-08

interface ServiceWorkerClients {
  Promise<Client?> getServiced();
  Promise<any> reloadAll();
};

// Extracted from https://slightlyoff.github.io/ServiceWorker/spec/service_worker/ on 2014-05-13

interface FetchEvent : Event {
  Promise<any> default();
};
