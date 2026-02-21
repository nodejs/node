let thePort = null;

export async function initialize(port) {
  port.postMessage('initialize');
  thePort = port;
}

export async function resolve(specifier, context, next) {
  if (specifier === 'node:fs' || specifier.includes('loader')) {
    return next(specifier);
  }

  thePort.postMessage(`resolve ${specifier}`);

  return next(specifier);
}
