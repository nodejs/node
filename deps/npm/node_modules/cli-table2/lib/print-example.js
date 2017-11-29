var chai = require('chai');
var expect = chai.expect;
var colors = require('colors/safe');
var _ = require('lodash');
var fs = require('fs');
var git = require('git-rev');

function logExample(fn){
  runPrintingExample(fn,
    function logName(name){
      console.log(colors.gray('=========  ') + name + colors.gray('  ================'));
    },
    console.log,  //logTable
    console.log,  //logCode
    console.log,  //logSeparator
    identity      //screenShot
  )
}

function mdExample(fn,file,cb){
  git.long(function(commitHash){
    var buffer = [];

    runPrintingExample(fn,
      function logName(name){
        buffer.push('##### ' + name);
      },
      function logTable(table){
        //md files won't render color strings properly.
        table = stripColors(table);

        // indent table so is displayed preformatted text
        table = '    ' + (table.split('\n').join('\n    '));

        buffer.push(table);
      },
      function logCode(code){
        buffer.push('```javascript');
        buffer.push(stripColors(code));
        buffer.push('```');
      },
      function logSeparator(sep){
        buffer.push(stripColors(sep));
      },
      function logScreenShot(image){
          buffer.push('![table image](https://cdn.rawgit.com/jamestalmage/cli-table2/'
                  + commitHash + '/examples/screenshots/' + image + '.png)');
      }
    );

    fs.writeFileSync(file,buffer.join('\n'));

    if(cb){cb();}
  });
}

/**
 * Actually runs the tests and verifies the functions create the expected output.
 * @param name of the test suite, used in the mocha.describe call.
 * @param fn a function which creates the test suite.
 */
function runTest(name,fn){
  function testExample(name,fn,expected){
    it(name,function(){
      expect(fn().toString()).to.equal(expected.join('\n'));
    });
  }

  describe(name,function () {
    fn(testExample,identity);
  });
}

/**
 * Common execution for runs that print output (either to console or to a Markdown file);
 * @param fn - a function containing the tests/demos.
 * @param logName - callback to print the name of this test to the console or file.
 * @param logTable - callback to print the output of the table to the console or file.
 * @param logCode - callback to print the output of the table to the console or file.
 * @param logSeparator - callback to print extra whitespace between demos.
 * @param logScreenShot - write out a link to the screenShot image.
 */
function runPrintingExample(fn,logName,logTable,logCode,logSeparator,logScreenShot){

  /**
   * Called by every test/demo
   * @param name - the name of this test.
   * @param makeTable - a function which builds the table under test. Also, The contents of this function will be printed.
   * @param expected -  The expected result.
   * @param screenshot - If present, there is an image containing a screenshot of the output
   */
  function printExample(name,makeTable,expected,screenshot){
    var code = makeTable.toString().split('\n').slice(1,-2).join('\n');

    logName(name);
    if(screenshot && logScreenShot){
      logScreenShot(screenshot);
    }
    else {
      logTable(makeTable().toString());
    }
    logCode(code);
    logSeparator('\n');
  }

  fn(printExample);
}

// removes all the color characters from a string
function stripColors(str){
  return str.split( /\u001b\[(?:\d*;){0,5}\d*m/g).join('');
}

// returns the first arg - used as snapshot function
function identity(str){
  return str;
}

module.exports = {
  mdExample:mdExample,
  logExample:logExample,
  runTest:runTest
};