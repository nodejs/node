const { Readable } = require('stream')

const r = new Readable();

readable.on('readable', function() {
  while (data = this.read()) {
    console.log(data);
  }
});
