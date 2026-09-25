// This worker expects a fetch event for a navigation and messages back the
// result of clients.get(event.resultingClientId).

// Resolves when the test finishes.
let testFinishPromise;
let resolveTestFinishPromise;
let rejectTestFinishPromise;

// Resolves to clients.get(event.resultingClientId) from the fetch event.
let getPromise;
let resolveGetPromise;
let rejectGetPromise;

let resultingClientId;

function startTest() {
  testFinishPromise = new Promise((resolve, reject) => {
    resolveTestFinishPromise = resolve;
    rejectTestFinishPromise = reject;
  });

  getPromise = new Promise((resolve, reject) => {
    resolveGetPromise = resolve;
    rejectGetPromise = reject;
  });
}

async function describeGetPromiseResult(promise) {
  const result = {};

  await promise.then(
    (client) => {
      result.promiseState = 'fulfilled';
      if (client === undefined) {
        result.promiseValue = 'undefinedValue';
      } else if (client instanceof Client) {
        result.promiseValue = 'client';
        result.client = {
          id:  client.id,
          url: client.url
        };
      } else {
        result.promiseValue = 'unknown';
      }
    },
    (error) => {
      result.promiseState = 'rejected';
    });

  return result;
}

async function handleGetResultingClient(event) {
  // Note that this message can arrive before |resultingClientId| is populated.
  const result = await describeGetPromiseResult(getPromise);
  // |resultingClientId| must be populated by now.
  result.queriedId = resultingClientId;
  event.source.postMessage(result);
};

async function handleGetClient(event) {
  const id = event.data.id;
  const result = await describeGetPromiseResult(self.clients.get(id));
  result.queriedId = id;
  event.source.postMessage(result);
};

self.addEventListener('message', (event) => {
  if (event.data.command == 'startTest') {
    startTest();
    event.waitUntil(testFinishPromise);
    event.source.postMessage('ok');
    return;
  }

  if (event.data.command == 'finishTest') {
    resolveTestFinishPromise();
    event.source.postMessage('ok');
    return;
  }

  if (event.data.command == 'getResultingClient') {
    event.waitUntil(handleGetResultingClient(event));
    return;
  }

  if (event.data.command == 'getClient') {
    event.waitUntil(handleGetClient(event));
    return;
  }
});

async function handleFetch(event) {
  try {
    resultingClientId = event.resultingClientId;
    const client = await self.clients.get(resultingClientId);
    resolveGetPromise(client);
  } catch (error) {
    rejectGetPromise(error);
  }
}

self.addEventListener('fetch', (event) => {
  if (event.request.mode != 'navigate')
    return;
  event.waitUntil(handleFetch(event));
});
