import { Transform } from 'node:stream';

export default class CoverageReporter extends Transform {
  constructor(options) {
    super({ ...options, writableObjectMode: true });
  }

  _transform(event, _encoding, callback) {
    if (event.type === 'test:coverage') {
      callback(null, JSON.stringify(event.data, null, 2));
    } else {
      callback(null);
    }
  }
}
