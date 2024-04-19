'use strict';

const StableREPL = require('internal/repl/stable/index');
const ExperimentalREPL = require('internal/repl/experimental/index');
const {
  getOptionValue,
} = require('internal/options');

const useExperimentalRepl = getOptionValue('--experimental-repl') || false;

module.exports = useExperimentalRepl ? ExperimentalREPL : StableREPL;
