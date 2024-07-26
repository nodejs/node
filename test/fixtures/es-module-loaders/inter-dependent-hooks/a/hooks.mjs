let port;

export function initialize(data) {
  ({ port } = data);
}

export function resolve(specifier, context, nextResolve) {
  const id = ''+Math.random();
  port.postMessage({
    context,
    id,
    specifier,
  });

  return new Promise((resolve) => {
    port.on('message', (message) => {
      if (message.id !== id) return;

      port.off('message', onMessage);
      resolve({ url: message.specifier });
    });
  });
}
