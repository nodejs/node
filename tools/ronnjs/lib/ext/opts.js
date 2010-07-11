/***************************************************************************
Author   : Joey Mazzarelli
Email    : mazzarelli@gmail.com
Homepage : http://joey.mazzarelli.com/js-opts
Source   : http://bitbucket.org/mazzarell/js-opts/
License  : Simplified BSD License
Version  : 1.0

Copyright 2010 Joey Mazzarelli. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY JOEY MAZZARELLI 'AS IS' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL JOEY MAZZARELLI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of Joey Mazzarelli.
***************************************************************************/

var puts        = require('sys').puts
  , values      = {}
  , args        = {}
  , argv        = []
  , errors      = []
  , descriptors = {opts:[], args:[]};

/**
 * Add a set of option descriptors, not yet ready to be parsed.
 * See exports.parse for description of options object
 *
 * Additionally, it takes a namespace as an argument, useful for
 * building options for a library in addition to the main app.
 */
exports.add = function (options, namespace) {
  for (var i=0; i<options.length; i++) {
    options[i].namespace = namespace;
    descriptors.opts.push(options[i]);
  }
};

/**
 * Parse the command line options
 * @param array options  Options to parse
 * @param array args     Arguments to parse
 * @param bool  help     Automatically generate help message, default false
 *
 * ===== Options Docs =====
 * Each option in the array can have the following fields. None are required, 
 * but you should at least provide a short or long name.
 *   {
 *     short       : 'l',
 *     long        : 'list',
 *     description : 'Show a list',
 *     value       : false,  // default false
 *     required    : true,   // default false
 *     callback    : function (value) { ... },
 *   }
 *
 * You can add an automatically generated help message by passing
 * a second parameter of <true> or by including the option;
 *   {
 *     long        : 'help',
 *     description : 'Show this help message',
 *     callback    : require('./opts').help,
 *   }
 *
 * ===== Arguments Docs =====
 * Arguments are different than options, and simpler. They typically come 
 * after the options, but the library really doesn't care. Each argument
 * can have the form of:
 *   {
 *     name     : 'script',
 *     required : true,      // default false
 *     callback : function (value) { ... },
 *   }
 */
exports.parse = function (options, params, help) {

  if (params === true) {
    help = true;
  } else if (!params) {
    params = [];
  } else {
    for (var i=0; i<params.length; i++) {
      descriptors.args.push(params[i]);
    }
  }

  if (help) {
    options.push({ long        : 'help'
                 , description : 'Show this help message'
                 , callback    : exports.help
                 });
  }
  for (var i=0; i<options.length; i++) {
    descriptors.opts.unshift(options[i]);
  }
  options = descriptors.opts;

  var checkDup = function (opt, type) {
    var prefix = (type == 'short')? '-': '--';
    var name = opt[type];
    if (!opts[prefix + name]) {
      opts[prefix + name] = opt;
    } else {
      if (opt.namespace && !opts[prefix + opt.namespace + '.' + name]) {
        opts[prefix + opt.namespace + '.' + name] = opt;
        for (var i=0; i<descriptors.opts.length; i++) {
          var desc = descriptors.opts[i];
          if (desc.namespace == opt.namespace) {
            if (type == 'long' && desc.long == opt.long) {
                descriptors.opts[i].long = opt.namespace + '.' + opt.long;
            } else if (type == 'short') {
              delete descriptors.opts[i].short;
            }
          }
        }
      } else {
        puts('Conflicting flags: ' + prefix + name + '\n');
        puts(helpString());
        process.exit(1);
      }
    }
  };

  var opts = {};
  for (var i=0; i<options.length; i++) {
    if (options[i].short) checkDup(options[i], 'short');
    if (options[i].long) checkDup(options[i], 'long');
  }

  for (var i=2; i<process.argv.length; i++) {
    var inp = process.argv[i];
    if (opts[inp]) {
      // found a match, process it.
      var opt = opts[inp];
      if (!opt.value) {
        if (opt.callback) opt.callback(true);
        if (opt.short) values[opt.short] = true;
        if (opt.long) values[opt.long] = true;
      } else {
        var next = process.argv[i+1];
        if (!next || opts[next]) {
          var flag = opt.short || opt.long;
          errors.push('Missing value for option: ' + flag);
          if (opt.short) values[opt.short] = true;
          if (opt.long) values[opt.long] = true;
        } else {
          if (opt.callback) opt.callback(next);
          if (opt.short) values[opt.short] = next;
          if (opt.long) values[opt.long] = next;
          i++;
        }
      }
    } else {
      // No match. If it starts with a dash, show an error. Otherwise
      // add it to the extra params.
      if (inp[0] == '-') {
        puts('Unknown option: ' + inp);
        if (opts['--help']) puts('Try --help');
        process.exit(1);
      } else {
        argv.push(inp);
        var arg = params.shift();
        if (arg) {
          args[arg.name] = inp;
          if (arg.callback) arg.callback(inp);
        }
      }
    }
  }
  for (var i=0; i<options.length; i++) {
    var flag = options[i].short || options[i].long;
    if (options[i].required && !exports.get(flag)) {
      errors.push('Missing required option: ' + flag);
    }
  }
  for (var i=0; i<params.length; i++) {
    if (params[i].required && !args[params[i].name]) {
      errors.push('Missing required argument: ' + params[i].name);
    }
  }
  if (errors.length) {
    for (var i=0; i<errors.length; i++) puts(errors[i]);
    puts('\n' + helpString());
    process.exit(1);
  }
};

/**
 * Get the value of an option. Can be the short or long option
 * @return string
 */
exports.get = function (opt) {
  return values[opt] || values['-' + opt] || values['--' + opt];
};

/**
 * Get unknown args. Could have special meaning to client
 */
exports.args = function () {
  return argv;
};

/**
 * Get an arg by name.
 * This only works if arg names were passed into the parse function.
 * @param string name Name of arg
 * @return string Value of arg
 */
exports.arg = function (name) {
  //puts(require('sys').inspect(arguments));
  return args[name];
};

/**
 * Print the help message and exit
 */
exports.help = function () {
  puts(helpString());
  process.exit(0);
};


// Create the help string
var helpString = function () {
  var str = 'Usage: ' + process.argv[0] + ' ' + process.argv[1];
  if (descriptors.opts.length) str += ' [options]';
  if (descriptors.args.length) {
    for (var i=0; i<descriptors.args.length; i++) {
      if (descriptors.args[i].required) {
        str += ' ' + descriptors.args[i].name;
      } else {
        str += ' [' + descriptors.args[i].name + ']';
      }
    }
  }
  str += '\n';
  for (var i=0; i<descriptors.opts.length; i++) {
    var opt = descriptors.opts[i];
    if (opt.description) str += (opt.description) + '\n';
    var line = '';
    if (opt.short && !opt.long) line += '-' + opt.short;
    else if (opt.long && !opt.short) line += '--' + opt.long;
    else line += '-' + opt.short + ', --' + opt.long;
    if (opt.value) line += ' <value>';
    if (opt.required) line += ' (required)';
    str += '    ' + line + '\n';
  }
  return str;
};
