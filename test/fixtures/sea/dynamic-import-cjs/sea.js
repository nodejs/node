const { pathToFileURL } = require('node:url');
const { dirname, join } = require('node:path');

const userURL = pathToFileURL(
  join(dirname(process.execPath), 'user.mjs'),
).href;

import(userURL)
  .then(({ message }) => {
    console.log(message);
  })
  .catch((err) => {
    console.error(err);
    process.exit(1);
  });
