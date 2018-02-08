describe('@api original-cli-table newline tests',function(){
  var Table = require('../src/table');
  var chai = require('chai');
  var expect = chai.expect;

  it('test table with newlines in headers', function() {
    var table = new Table({
      head: ['Test', "1\n2\n3"]
      , style: {
        'padding-left': 1
        , 'padding-right': 1
        , head: []
        , border: []
      }
    });

    var expected = [
        '┌──────┬───┐'
      , '│ Test │ 1 │'
      , '│      │ 2 │'
      , '│      │ 3 │'
      , '└──────┴───┘'
    ];

    expect(table.toString()).to.equal(expected.join("\n"));
  });

  it('test column width is accurately reflected when newlines are present', function() {
    var table = new Table({ head: ['Test\nWidth'], style: {head:[], border:[]} });
    expect(table.width).to.equal(9);
  });

  it('test newlines in body cells', function() {
    var table = new Table({style: {head:[], border:[]}});

    table.push(["something\nwith\nnewlines"]);

    var expected = [
        '┌───────────┐'
      , '│ something │'
      , '│ with      │'
      , '│ newlines  │'
      , '└───────────┘'
    ];

    expect(table.toString()).to.equal(expected.join("\n"));
  });

  it('test newlines in vertical cell header and body', function() {
    var table = new Table({ style: {'padding-left':0, 'padding-right':0, head:[], border:[]} });

    table.push(
      {'v\n0.1': 'Testing\nsomething cool'}
    );

    var expected = [
        '┌───┬──────────────┐'
      , '│v  │Testing       │'
      , '│0.1│something cool│'
      , '└───┴──────────────┘'
    ];

    expect(table.toString()).to.equal(expected.join("\n"));
  });

  it('test newlines in cross table header and body', function() {
    var table = new Table({ head: ["", "Header\n1"], style: {'padding-left':0, 'padding-right':0, head:[], border:[]} });

    table.push({ "Header\n2": ['Testing\nsomething\ncool'] });

    var expected = [
        '┌──────┬─────────┐'
      , '│      │Header   │'
      , '│      │1        │'
      , '├──────┼─────────┤'
      , '│Header│Testing  │'
      , '│2     │something│'
      , '│      │cool     │'
      , '└──────┴─────────┘'
    ];

    expect(table.toString()).to.equal(expected.join("\n"));
  });

});

