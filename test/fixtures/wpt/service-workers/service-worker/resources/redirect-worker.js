// We store an empty response for each fetch event request we see
// in this Cache object so we can get the list of urls in the
// message event.
var cacheName = 'urls-' + self.registration.scope;

var waitUntilPromiseList = [];

// Sends the requests seen by this worker. The output is:
// {
//   requestInfos: [
//     {url: url1, resultingClientId: id1},
//     {url: url2, resultingClientId: id2},
//   ]
// }
async function getRequestInfos(event) {
  // Wait for fetch events to finish.
  await Promise.all(waitUntilPromiseList);
  waitUntilPromiseList = [];

  // Generate the message.
  const cache = await caches.open(cacheName);
  const requestList = await cache.keys();
  const requestInfos = [];
  for (let i = 0; i < requestList.length; i++) {
    const response = await cache.match(requestList[i]);
    const body = await response.json();
    requestInfos[i] = {
      url: requestList[i].url,
      resultingClientId: body.resultingClientId
    };
  }
  await caches.delete(cacheName);

  event.data.port.postMessage({requestInfos});
}

// Sends the results of clients.get(id) from this worker. The
// input is:
// {
//   actual_ids: {a: id1, b: id2, x: id3}
// }
//
// The output is:
// {
//   clients: {
//     a: {found: false},
//     b: {found: false},
//     x: {
//       id: id3,
//       url: url1,
//       found: true
//    }
//   }
// }
async function getClients(event) {
  // |actual_ids| is like:
  // {a: id1, b: id2, x: id3}
  const actual_ids = event.data.actual_ids;
  const result = {}
  for (let key of Object.keys(actual_ids)) {
    const id = actual_ids[key];
    const client = await self.clients.get(id);
    if (client === undefined)
      result[key] = {found: false};
    else
      result[key] = {found: true, url: client.url, id: client.id};
  }
  event.data.port.postMessage({clients: result});
}

self.addEventListener('message', async function(event) {
  if (event.data.command == 'getRequestInfos') {
    event.waitUntil(getRequestInfos(event));
    return;
  }

  if (event.data.command == 'getClients') {
    event.waitUntil(getClients(event));
    return;
  }
});

function get_query_params(url) {
  var search = (new URL(url)).search;
  if (!search) {
    return {};
  }
  var ret = {};
  var params = search.substring(1).split('&');
  params.forEach(function(param) {
      var element = param.split('=');
      ret[decodeURIComponent(element[0])] = decodeURIComponent(element[1]);
    });
  return ret;
}

self.addEventListener('fetch', function(event) {
    var waitUntilPromise = caches.open(cacheName).then(function(cache) {
      const responseBody = {};
      responseBody['resultingClientId'] = event.resultingClientId;
      const headers = new Headers({'Content-Type': 'application/json'});
      const response = new Response(JSON.stringify(responseBody), {headers});
      return cache.put(event.request, response);
    });
    event.waitUntil(waitUntilPromise);

    var params = get_query_params(event.request.url);
    if (!params['sw']) {
      // To avoid races, add the waitUntil() promise to our global list.
      // If we get a message event before we finish here, it will wait
      // these promises to complete before proceeding to read from the
      // cache.
      waitUntilPromiseList.push(waitUntilPromise);
      return;
    }

    event.respondWith(waitUntilPromise.then(async () => {
      if (params['sw'] == 'gen') {
        return Response.redirect(params['url']);
      } else if (params['sw'] == 'gen-manual') {
        // Note this differs from Response.redirect() in that relative URLs are
        // preserved.
        return new Response("", {
          status: 301,
          headers: {location: params['url']},
        });
      } else if (params['sw'] == 'fetch') {
        return fetch(event.request);
      } else if (params['sw'] == 'fetch-url') {
        return fetch(params['url']);
      } else if (params['sw'] == 'follow') {
        return fetch(new Request(event.request.url, {redirect: 'follow'}));
      } else if (params['sw'] == 'manual') {
        return fetch(new Request(event.request.url, {redirect: 'manual'}));
      } else if (params['sw'] == 'manualThroughCache') {
        const url = event.request.url;
        await caches.delete(url)
        const cache = await self.caches.open(url);
        const response = await fetch(new Request(url, {redirect: 'manual'}));
        await cache.put(event.request, response);
        return cache.match(url);
      }
      // unexpected... trigger an interception failure
    }));
  });
