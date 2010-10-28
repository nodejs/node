## Global Objects

These object are available in the global scope and can be accessed from anywhere.

### global

The global namespace object.

### process

The process object. See the `'process object'` section.

### require()

To require modules. See the `'Modules'` section.

### require.resolve()

Use the internal `require()` machinery to look up the location of a module,
but rather than loading the module, just return the resolved filename.

### require.paths

An array of search paths for `require()`.  This array can be modified to add
custom paths.

Example: add a new path to the beginning of the search list

    require.paths.unshift('/usr/local/node');


### __filename

The filename of the script being executed.  This is the absolute path, and not necessarily
the same filename passed in as a command line argument.

Example: running `node example.js` from `/Users/mjr`

    console.log(__filename);
    // /Users/mjr/example.js

### __dirname

The dirname of the script being executed.

Example: running `node example.js` from `/Users/mjr`

    console.log(__dirname);
    // /Users/mjr


### module

A reference to the current module (of type `process.Module`). In particular
`module.exports` is the same as the `exports` object. See `src/process.js`
for more information.
