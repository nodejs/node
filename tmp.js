const { Readable } = require('stream');
const { Buffer } = require('buffer');

const points = [];

for (let x = 0; x < 10; x++)
  points[x] = '' + x;

let buf = Buffer.alloc(12);
let i = 0, offset = 0;

const stream = new Readable({
  read() {
    for (; i < points.length; i++) {
      const str = points[i];
      if (str.length + offset + 1 >= 12) {
        this.push(buf.slice(0, offset), 'utf8');
        buf = Buffer.alloc(12);
        offset = 0;
        continue;
      }
      buf.write(str, offset, str.length, 'utf8');
      offset += str.length;
      buf.write('\n', offset, 1, 'utf8');
      offset++;
    }
    this.push(buf.slice(0, offset), 'utf8');
    this.push(null);
  }
});

stream.pipe(process.stdout)
