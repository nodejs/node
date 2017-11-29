var Table = require('../src/table');
var colors = require('colors/safe');

module.exports = function(runTest) {

  function it(name, fn) {
    var result = fn();
    runTest(name, result[0], result[1], result[2]);
  }

  it('Basic Usage',function(){
    function makeTable(){
      // By default, headers will be red, and borders will be grey
      var table = new Table({head:['a','b']});

      table.push(['c','d']);

      return table;
    }

    var expected = [
        colors.gray('┌───')                                                         + colors.gray('┬───┐')
      , colors.gray('│') + colors.red(' a ') + colors.gray('│') + colors.red(' b ')     + colors.gray('│')
      , colors.gray('├───') +                                                         colors.gray('┼───┤')
      , colors.gray('│') +           (' c ') + colors.gray('│') +          (' d ') +      colors.gray('│')
      , colors.gray('└───')                                                         + colors.gray('┴───┘')
    ];

    return [makeTable,expected,'basic-usage-with-colors'];
  });

  it('Basic Usage - disable colors - (used often in the examples and tests)', function (){
    function makeTable(){
      // For most of these examples, and most of the unit tests we disable colors.
      // It makes unit tests easier to write/understand, and allows these pages to
      // display the examples as text instead of screen shots.
      var table = new Table({
           head: ['Rel', 'Change', 'By', 'When']
        , style: {
            head: []    //disable colors in header cells
          , border: []  //disable colors for the border
        }
        , colWidths: [6, 21, 25, 17]  //set the widths of each column (optional)
      });

      table.push(
          ['v0.1', 'Testing something cool', 'rauchg@gmail.com', '7 minutes ago']
        , ['v0.1', 'Testing something cool', 'rauchg@gmail.com', '8 minutes ago']
      );

      return table;
    }

    var expected = [
        '┌──────┬─────────────────────┬─────────────────────────┬─────────────────┐'
      , '│ Rel  │ Change              │ By                      │ When            │'
      , '├──────┼─────────────────────┼─────────────────────────┼─────────────────┤'
      , '│ v0.1 │ Testing something … │ rauchg@gmail.com        │ 7 minutes ago   │'
      , '├──────┼─────────────────────┼─────────────────────────┼─────────────────┤'
      , '│ v0.1 │ Testing something … │ rauchg@gmail.com        │ 8 minutes ago   │'
      , '└──────┴─────────────────────┴─────────────────────────┴─────────────────┘'
    ];

    return [makeTable,expected];
  });

  it('Create vertical tables by adding objects a that specify key-value pairs', function() {
    function makeTable(){
      var table = new Table({ style: {'padding-left':0, 'padding-right':0, head:[], border:[]} });

      table.push(
          {'v0.1': 'Testing something cool'}
        , {'v0.1': 'Testing something cool'}
      );

      return table;
    }

    var expected = [
        '┌────┬──────────────────────┐'
      , '│v0.1│Testing something cool│'
      , '├────┼──────────────────────┤'
      , '│v0.1│Testing something cool│'
      , '└────┴──────────────────────┘'
    ];

    return [makeTable,expected];
  });

  it('Cross tables are similar to vertical tables, but include an empty string for the first header', function() {
    function makeTable(){
      var table = new Table({ head: ["", "Header 1", "Header 2"], style: {'padding-left':0, 'padding-right':0, head:[], border:[]} }); // clear styles to prevent color output

      table.push(
        {"Header 3": ['v0.1', 'Testing something cool'] }
        , {"Header 4": ['v0.1', 'Testing something cool'] }
      );

      return table;
    }

    var expected = [
        '┌────────┬────────┬──────────────────────┐'
      , '│        │Header 1│Header 2              │'
      , '├────────┼────────┼──────────────────────┤'
      , '│Header 3│v0.1    │Testing something cool│'
      , '├────────┼────────┼──────────────────────┤'
      , '│Header 4│v0.1    │Testing something cool│'
      , '└────────┴────────┴──────────────────────┘'
    ];

    return [makeTable,expected];
  });

  it('Stylize the table with custom border characters', function (){
    function makeTable(){
      var table = new Table({
        chars: {
          'top': '═'
          , 'top-mid': '╤'
          , 'top-left': '╔'
          , 'top-right': '╗'
          , 'bottom': '═'
          , 'bottom-mid': '╧'
          , 'bottom-left': '╚'
          , 'bottom-right': '╝'
          , 'left': '║'
          , 'left-mid': '╟'
          , 'right': '║'
          , 'right-mid': '╢'
        },
        style: {
          head: []
          , border: []
        }
      });

      table.push(
        ['foo', 'bar', 'baz']
        , ['frob', 'bar', 'quuz']
      );

      return table;
    }

    var expected = [
        '╔══════╤═════╤══════╗'
      , '║ foo  │ bar │ baz  ║'
      , '╟──────┼─────┼──────╢'
      , '║ frob │ bar │ quuz ║'
      , '╚══════╧═════╧══════╝'
    ];

    return [makeTable,expected];
  });

  it('Use ansi colors (i.e. colors.js) to style text within the cells at will, even across multiple lines',function(){
    function makeTable(){
      var table = new Table({style:{border:[],header:[]}});

      table.push([
        colors.red('Hello\nhow\nare\nyou?'),
        colors.blue('I\nam\nfine\nthanks!')
      ]);

      return table;
    }

    var expected = [
        '┌───────┬─────────┐'
      , '│ ' + colors.red('Hello') + ' │ ' + colors.blue('I') + '       │'
      , '│ ' + colors.red('how') + '   │ ' + colors.blue('am') + '      │'
      , '│ ' + colors.red('are') + '   │ ' + colors.blue('fine') + '    │'
      , '│ ' + colors.red('you?') + '  │ ' + colors.blue('thanks!') + ' │'
      , '└───────┴─────────┘'
    ];

    return [makeTable,expected,'multi-line-colors'];
  });

  it('Set `wordWrap` to true to make lines of text wrap instead of being truncated',function(){
    function makeTable(){
      var table = new Table({
        style:{border:[],header:[]},
        colWidths:[7,9],
        wordWrap:true
      });

      table.push([
        'Hello how are you?',
        'I am fine thanks!'
      ]);

      return table;
    }

    var expected = [
        '┌───────┬─────────┐'
      , '│ Hello │ I am    │'
      , '│ how   │ fine    │'
      , '│ are   │ thanks! │'
      , '│ you?  │         │'
      , '└───────┴─────────┘'
    ];

    return [makeTable,expected];
  });
};

/* Expectation - ready to be copy/pasted and filled in. DO NOT DELETE THIS


 var expected = [
   '┌──┬───┬──┬──┐'
 , '│  │   │  │  │'
 , '├──┼───┼──┼──┤'
 , '│  │ … │  │  │'
 , '├──┼───┼──┼──┤'
 , '│  │ … │  │  │'
 , '└──┴───┴──┴──┘'
 ];
 */