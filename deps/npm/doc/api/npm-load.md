npm-load(3) -- Load config settings
===================================

## SYNOPSIS

    npm.load(conf, cb)

## DESCRIPTION

npm.load() must be called before any other function call.  Both parameters are
optional, but the second is recommended.

The first parameter is an object containing command-line config params, and the
second parameter is a callback that will be called when npm is loaded and ready
to serve.

The first parameter should follow a similar structure as the package.json
config object.

For example, to emulate the --dev flag, pass an object that looks like this:

    {
      "dev": true
    }

For a list of all the available command-line configs, see `npm help config`
