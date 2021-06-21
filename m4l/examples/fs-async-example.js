const fs = require('fs');
const path = require('path');

(async () => {
  const data = await fs.promises.readFile(path.resolve(__dirname, 'fs-async-example.js'));

  console.log(data);
})();
