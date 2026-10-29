let globalAddRoutes;
let addRoutesError = {};

self.addEventListener('install', event => {
  globalAddRoutes = event.addRoutes.bind(event);
  globalAddRoutes([
    {
      condition: { urlPattern: '/', runningStatus: 'not-running' },
      source: 'network',
    },
  ])
    .then(() => {
      addRoutesError.install = null;
  })
    .catch(error => {
      addRoutesError.install = error;
    });
});

self.addEventListener('activate', event => {
  globalAddRoutes([
    {
      condition: { urlPattern: '/', runningStatus: 'not-running' },
      source: 'network',
    },
  ])
    .then(() => {
      addRoutesError.activate = null;
    })
    .catch(error => {
      addRoutesError.activate = error;
    });
});

self.addEventListener('message', event => {
  event.ports[0].postMessage(addRoutesError);
});