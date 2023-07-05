import { get } from 'http';

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

  return nextLoad(url);
}
