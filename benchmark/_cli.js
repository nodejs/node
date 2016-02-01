'use strict';

const fs = require('fs');
const path = require('path');

// Create an object of all benchmark scripts
const benchmarks = {};
fs.readdirSync(__dirname)
  .filter(function(name) {
    return fs.statSync(path.resolve(__dirname, name)).isDirectory();
  })
  .forEach(function(category) {
    benchmarks[category] = fs.readdirSync(path.resolve(__dirname, category))
      .filter((filename) => filename[0] !== '.' && filename[0] !== '_');
  });

function CLI(usage, settings) {
  if (!(this instanceof CLI)) return new CLI(usage, settings);

  if (process.argv.length < 3) {
    this.abort(usage); // abort will exit the process
  }

  this.usage = usage;
  this.optional = {};
  this.items = [];

  for (const argName of settings.arrayArgs) {
    this.optional[argName] = [];
  }

  let currentOptional = null;
  let mode = 'both'; // possible states are: [both, option, item]

  for (const arg of process.argv.slice(2)) {
    if (arg === '--') {
      // Only items can follow --
      mode = 'item';
    } else if (['both', 'option'].includes(mode) && arg[0] === '-') {
      // Optional arguments declaration

      if (arg[1] === '-') {
        currentOptional = arg.slice(2);
      } else {
        currentOptional = arg.slice(1);
      }

      // Default the value to true
      if (!settings.arrayArgs.includes(currentOptional)) {
        this.optional[currentOptional] = true;
      }

      // expect the next value to be option related (either -- or the value)
      mode = 'option';
    } else if (mode === 'option') {
      // Optional arguments value

      if (settings.arrayArgs.includes(currentOptional)) {
        this.optional[currentOptional].push(arg);
      } else {
        this.optional[currentOptional] = arg;
      }

      // the next value can be either an option or an item
      mode = 'both';
    } else if (['both', 'item'].includes(mode)) {
      // item arguments
      this.items.push(arg);

      // the next value must be an item
      mode = 'item';
    } else {
      // Bad case, abort
      this.abort(usage);
      return;
    }
  }
}
module.exports = CLI;

CLI.prototype.abort = function(msg) {
  console.error(msg);
  process.exit(1);
};

CLI.prototype.benchmarks = function() {
  const paths = [];
  const filter = this.optional.filter || false;

  for (const category of this.items) {
    for (const scripts of benchmarks[category]) {
      if (filter && scripts.lastIndexOf(filter) === -1) continue;

      paths.push(path.join(category, scripts));
    }
  }

  return paths;
};
