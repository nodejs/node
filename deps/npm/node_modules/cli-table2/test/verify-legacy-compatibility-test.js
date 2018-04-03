(function(){
  describe('verify original cli-table behavior',function(){
     commonTests(require('cli-table'));
  });

  describe('@api cli-table2 matches verified behavior',function(){
     commonTests(require('../src/table'));
  });

  function commonTests(Table){
    var chai = require('chai');
    var expect = chai.expect;
    var colors = require('colors/safe');

    it('empty table has a width of 0',function(){
      var table = new Table();
      expect(table.width).to.equal(0);
      expect(table.toString()).to.equal('');
    });

    it('header text will be colored according to style',function(){
      var table = new Table({head:['hello','goodbye'],style:{border:[],head:['red','bgWhite']}});

      var expected = [
          '┌───────┬─────────┐'
        , '│' + colors.bgWhite.red(' hello ') +'│' + colors.bgWhite.red(' goodbye ') + '│'
        , '└───────┴─────────┘'
      ];

      expect(table.toString()).to.equal(expected.join('\n'));
    });

    it('tables with one row of data will not be treated as headers',function(){
      var table = new Table({style:{border:[],head:['red']}});

      table.push(['hello','goodbye']);

      var expected = [
          '┌───────┬─────────┐'
        , '│ hello │ goodbye │'
        , '└───────┴─────────┘'
      ];

      expect(table.toString()).to.equal(expected.join('\n'));
    });

    it('table with headers and data headers',function(){
      var table = new Table({head:['hello','goodbye'],style:{border:[],head:['red']}});

      table.push(['hola','adios']);

      var expected = [
          '┌───────┬─────────┐'
        , '│' + colors.red(' hello ') +'│' + colors.red(' goodbye ') + '│'
        , '├───────┼─────────┤'
        , '│ hola  │ adios   │'
        , '└───────┴─────────┘'
      ];

      expect(table.toString()).to.equal(expected.join('\n'));
    });

    it('compact shorthand',function(){
      var table = new Table({style:{compact:true,border:[],head:['red']}});

      table.push(['hello','goodbye'],['hola','adios']);

      var expected = [
          '┌───────┬─────────┐'
        , '│ hello │ goodbye │'
        , '│ hola  │ adios   │'
        , '└───────┴─────────┘'
      ];

      expect(table.toString()).to.equal(expected.join('\n'));
    });

    it('compact shorthand - headers are still rendered with separator',function(){
      var table = new Table({head:['hello','goodbye'],style:{compact:true,border:[],head:[]}});

      table.push(['hola','adios'],['hi','bye']);

      var expected = [
          '┌───────┬─────────┐'
        , '│ hello │ goodbye │'
        , '├───────┼─────────┤'
        , '│ hola  │ adios   │'
        , '│ hi    │ bye     │'
        , '└───────┴─────────┘'
      ];

      expect(table.toString()).to.equal(expected.join('\n'));
    });

    it('compact longhand - headers are not rendered with separator',function(){
      var table = new Table({
        chars: {
          'mid': ''
          , 'left-mid': ''
          , 'mid-mid': ''
          , 'right-mid': ''
        },
        head:['hello','goodbye'],
        style:{border:[],head:[]}}
      );

      table.push(['hola','adios'],['hi','bye']);

      var expected = [
          '┌───────┬─────────┐'
        , '│ hello │ goodbye │'
        , '│ hola  │ adios   │'
        , '│ hi    │ bye     │'
        , '└───────┴─────────┘'
      ];

      expect(table.toString()).to.equal(expected.join('\n'));
    });

    it('compact longhand',function(){
      var table = new Table({
        chars: {
          'mid': ''
          , 'left-mid': ''
          , 'mid-mid': ''
          , 'right-mid': ''
        },
        style:{border:[],head:['red']}
      });

      table.push(['hello','goodbye'],['hola','adios']);

      var expected = [
          '┌───────┬─────────┐'
        , '│ hello │ goodbye │'
        , '│ hola  │ adios   │'
        , '└───────┴─────────┘'
      ];

      expect(table.toString()).to.equal(expected.join('\n'));
    });

    it('objects with multiple properties in a cross-table',function(){
      var table = new Table({style:{border:[],head:[]}});

      table.push(
        {'a':['b'], c:['d']}   // value of property 'c' will be discarded
      );

      var expected = [
          '┌───┬───┐'
        , '│ a │ b │'
        , '└───┴───┘'
      ];
      expect(table.toString()).to.equal(expected.join('\n'));
    });
  }
})();