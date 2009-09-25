// A repl library that you can include in your own code to get a runtime
// interface to your program. Just require("/repl.js").

puts("Type '.help' for options.");

node.stdio.open();
node.stdio.addListener("data", readline);

var buffered_cmd = '';
var trimmer = /^\s*(.+)\s*$/m;
var scopedVar = /^\s*var\s*([_\w\$]+)(.*)$/m;
var scopeFunc = /^\s*function\s*([_\w\$]+)/;

exports.prompt = "node> ";
exports.scope = {};

displayPrompt();


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
        exports.scope['_'] = ret;
        printValue(ret);
      }
        
      buffered_cmd = '';
    } catch (e) {
      if (!(e instanceof SyntaxError)) 
        throw e;
    }
  } catch (e) {
    // On error: Print the error and clear the buffer
    puts('caught an exception: ' + e);
    buffered_cmd = '';
  }
  
  displayPrompt();
}


/**
 * Used to display the prompt. 
 */
function displayPrompt () {
  print(buffered_cmd.length ? '...   ' : exports.prompt);
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
    puts("Clearing Scope...");
    buffered_cmd = '';
    exports.scope = {};
    displayPrompt();
    return true;
  case ".exit":
    node.stdio.close();
    return true;
  case ".help":
    puts(".break\tSometimes you get stuck in a place you can't get out... This will get you out.");
    puts(".clear\tBreak, and also clear the local scope.");
    puts(".exit\tExit the prompt");
    puts(".help\tShow repl options");
    displayPrompt();
    return true;
  }
  return 0;
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

/**
 * Echos the value of a value. Trys to print the value out
 * in the best way possible given the different types.
 * 
 * @param {Object} value The object to print out
 */
function printValue (value) {
  if (value === 0) {
    puts("0");
    return;
  }
  
  if (value === false) {
    puts("false");
    return;
  }
  
  if (value === "") {
    puts('""');
    return;
  }
  
  if (typeof(value) == "function") {
    puts("[Function]");
    return;
  }
  
  try {
    puts(JSON.stringify(value));
  } catch (e) {
    if (e.message.search("circular"))
      puts("[Circular Object]");
    else
      throw e;
  }
}
