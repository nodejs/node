// A repl library that you can include in your own code to get a runtime
// interface to your program. Just require("/repl.js").

var sys = require('sys');

var buffered_cmd = '';
var trimmer = /^\s*(.+)\s*$/m;
var scopedVar = /^\s*var\s*([_\w\$]+)(.*)$/m;
var scopeFunc = /^\s*function\s*([_\w\$]+)/;

exports.scope = {};
exports.prompt = "node> ";
// Can overridden with custom print functions, such as `probe` or `eyes.js`
exports.writer = sys.p;
exports.start = function (prompt) {
  if (prompt !== undefined) {
    exports.prompt = prompt;
  }

  process.stdio.open();
  process.stdio.addListener("data", readline);
  displayPrompt();
}

/**
 * The main REPL function. This is called everytime the user enters
 * data on the command line.
 */
function readline (cmd) {
  cmd = trimWhitespace(cmd);
  
  // Check to see if a REPL keyword was used. If it returns true,
  // display next prompt and return.
  if (parseREPLKeyword(cmd) === true) {
    return;
  }
  
  // The catchall for errors
  try {
    buffered_cmd += cmd;
    // This try is for determining if the command is complete, or should
    // continue onto the next line.
    try {
      buffered_cmd = convertToScope(buffered_cmd);
      
      // Scope the readline with exports.scope to provide "local" vars
      with (exports.scope) {
        var ret = eval(buffered_cmd);
        if (ret !== undefined) {
          exports.scope['_'] = ret;
          exports.writer(ret);
        }
      }
        
      buffered_cmd = '';
    } catch (e) {
      if (!(e instanceof SyntaxError)) throw e;
    }
  } catch (e) {
    // On error: Print the error and clear the buffer
    if (e.stack) {
      sys.puts(e.stack);
    } else {
      sys.puts(e.toString());
    }
    buffered_cmd = '';
  }
  
  displayPrompt();
}


/**
 * Used to display the prompt. 
 */
function displayPrompt () {
  sys.print(buffered_cmd.length ? '...   ' : exports.prompt);
}

/**
 * Used to parse and execute the Node REPL commands.
 * 
 * @param {cmd} cmd The command entered to check
 * @returns {Boolean} If true it means don't continue parsing the command 
 */
function parseREPLKeyword (cmd) {
  switch (cmd) {
  case ".break":
    buffered_cmd = '';
    displayPrompt();
    return true;
  case ".clear":
    sys.puts("Clearing Scope...");
    buffered_cmd = '';
    exports.scope = {};
    displayPrompt();
    return true;
  case ".exit":
    process.stdio.close();
    return true;
  case ".help":
    sys.puts(".break\tSometimes you get stuck in a place you can't get out... This will get you out.");
    sys.puts(".clear\tBreak, and also clear the local scope.");
    sys.puts(".exit\tExit the prompt");
    sys.puts(".help\tShow repl options");
    displayPrompt();
    return true;
  }
  return false;
}

/**
 * Trims Whitespace from a line.
 * 
 * @param {String} cmd The string to trim the whitespace from
 * @returns {String} The trimmed string 
 */
function trimWhitespace (cmd) {
  var matches = trimmer.exec(cmd);
  if (matches && matches.length == 2) {
    return matches[1];
  }
}

/**
 * Converts commands that use var and function <name>() to use the
 * local exports.scope when evaled. This provides a local scope
 * on the REPL.
 * 
 * @param {String} cmd The cmd to convert
 * @returns {String} The converted command
 */
function convertToScope (cmd) {
  var matches;
  
  // Replaces: var foo = "bar";  with: exports.scope.foo = bar;
  matches = scopedVar.exec(cmd);
  if (matches && matches.length == 3) {
    return "exports.scope." + matches[1] + matches[2];
  }
  
  // Replaces: function foo() {};  with: foo = function foo() {};
  matches = scopeFunc.exec(buffered_cmd);
  if (matches && matches.length == 2) {
    return matches[1] + " = " + buffered_cmd;
  }
  
  return cmd;
}
