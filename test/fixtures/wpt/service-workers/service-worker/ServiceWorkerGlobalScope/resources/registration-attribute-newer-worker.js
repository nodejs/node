// TODO(nhiroki): stop using global states because service workers can be killed
// at any point. Instead, we could post a message to the page on each event via
// Client object (http://crbug.com/558244).
var results = [];

function stringify(worker) {
  return worker ? worker.scriptURL : 'empty';
}

function record(event_name) {
  results.push(event_name);
  results.push('  installing: ' + stringify(self.registration.installing));
  results.push('  waiting: ' + stringify(self.registration.waiting));
  results.push('  active: ' + stringify(self.registration.active));
}

record('evaluate');

self.registration.addEventListener('updatefound', function() {
    record('updatefound');
    var worker = self.registration.installing;
    self.registration.installing.addEventListener('statechange', function() {
        record('statechange(' + worker.state + ')');
      });
  });

self.addEventListener('install', function(e) { record('install'); });

self.addEventListener('activate', function(e) { record('activate'); });

self.addEventListener('message', function(e) {
    e.data.port.postMessage(results);
  });
