import { get } from 'http';

export function resolve(specifier, context, nextResolve) {
  const { parentURL = null } = context;

  if (specifier.startsWith('http://')) {
    return {
      shortCircuit: true,
      url: specifier,
    };
  } else if (parentURL?.startsWith('http://')) {
    return {
      shortCircuit: true,
      url: new URL(specifier, parentURL).href,
    };
  }

  return nextResolve(specifier, context);
}

export function load(url, context, nextLoad) {
  if (url.startsWith('http://')) {
    return new Promise((resolve, reject) => {
      get(url, (rsp) => {
        let data = '';
        rsp.on('data', (chunk) => data += chunk);
        rsp.on('end', () => {
          resolve({
            format: 'module',
            shortCircuit: true,
            source: data,
          });
        });
      })
      .on('error', reject);
    });
  }

  return nextLoad(url, context);
}
