describe('Cell',function(){
  var chai = require('chai');
  var expect = chai.expect;
  var sinon = require('sinon');
  var sinonChai = require("sinon-chai");
  chai.use(sinonChai);

  var colors = require('colors');
  var Cell = require('../src/cell');
  var RowSpanCell = Cell.RowSpanCell;
  var ColSpanCell = Cell.ColSpanCell;
  var mergeOptions = require('../src/utils').mergeOptions;

  function defaultOptions(){
    //overwrite coloring of head and border by default for easier testing.
    return mergeOptions({style:{head:[],border:[]}});
  }

  function defaultChars(){
    return {
      'top': '─'
      , 'topMid': '┬'
      , 'topLeft': '┌'
      , 'topRight': '┐'
      , 'bottom': '─'
      , 'bottomMid': '┴'
      , 'bottomLeft': '└'
      , 'bottomRight': '┘'
      , 'left': '│'
      , 'leftMid': '├'
      , 'mid': '─'
      , 'midMid': '┼'
      , 'right': '│'
      , 'rightMid': '┤'
      , 'middle': '│'
    };
  }

  describe('constructor',function(){
    it('colSpan and rowSpan default to 1',function(){
      var cell = new Cell();
      expect(cell.colSpan).to.equal(1);
      expect(cell.rowSpan).to.equal(1);
    });

    it('colSpan and rowSpan can be set via constructor',function(){
      var cell = new Cell({rowSpan:2,colSpan:3});
      expect(cell.rowSpan).to.equal(2);
      expect(cell.colSpan).to.equal(3);
    });

    it('content can be set as a string',function(){
      var cell = new Cell('hello\nworld');
      expect(cell.content).to.equal('hello\nworld');
    });

    it('content can be set as a options property',function(){
      var cell = new Cell({content:'hello\nworld'});
      expect(cell.content).to.equal('hello\nworld');
    });

    it('default content is an empty string',function(){
      var cell = new Cell();
      expect(cell.content).to.equal('');
    });

    it('new Cell(null) will have empty string content',function(){
      var cell = new Cell(null);
      expect(cell.content).to.equal('');
    });

    it('new Cell({content: null}) will have empty string content',function(){
      var cell = new Cell({content: null});
      expect(cell.content).to.equal('');
    });

    it('new Cell(0) will have "0" as content',function(){
      var cell = new Cell(0);
      expect(cell.content).to.equal('0');
    });

    it('new Cell({content: 0}) will have "0" as content',function(){
      var cell = new Cell({content: 0});
      expect(cell.content).to.equal('0');
    });

    it('new Cell(false) will have "false" as content',function(){
      var cell = new Cell(false);
      expect(cell.content).to.equal('false');
    });

    it('new Cell({content: false}) will have "false" as content',function(){
      var cell = new Cell({content: false});
      expect(cell.content).to.equal('false');
    });
  });

  describe('mergeTableOptions',function(){
    describe('chars',function(){
      it('unset chars take on value of table',function(){
        var cell = new Cell();
        var tableOptions = defaultOptions();
        cell.mergeTableOptions(tableOptions);
        expect(cell.chars).to.eql(defaultChars());
      });

      it('set chars override the value of table',function(){
        var cell = new Cell({chars:{bottomRight:'='}});
        cell.mergeTableOptions(defaultOptions());
        var chars = defaultChars();
        chars.bottomRight = '=';
        expect(cell.chars).to.eql(chars);
      });

      it('hyphenated names will be converted to camel-case',function(){
        var cell = new Cell({chars:{'bottom-left':'='}});
        cell.mergeTableOptions(defaultOptions());
        var chars = defaultChars();
        chars.bottomLeft = '=';
        expect(cell.chars).to.eql(chars);
      });
    });

    describe('truncate',function(){
      it('if unset takes on value of table',function(){
        var cell = new Cell();
        cell.mergeTableOptions(defaultOptions());
        expect(cell.truncate).to.equal('…');
      });

      it('if set overrides value of table',function(){
        var cell = new Cell({truncate:'...'});
        cell.mergeTableOptions(defaultOptions());
        expect(cell.truncate).to.equal('...');
      });
    });

    describe('style.padding-left', function () {
      it('if unset will be copied from tableOptions.style', function () {
        var cell = new Cell();
        cell.mergeTableOptions(defaultOptions());
        expect(cell.paddingLeft).to.equal(1);

        cell = new Cell();
        var tableOptions = defaultOptions();
        tableOptions.style['padding-left'] = 2;
        cell.mergeTableOptions(tableOptions);
        expect(cell.paddingLeft).to.equal(2);

        cell = new Cell();
        tableOptions = defaultOptions();
        tableOptions.style.paddingLeft = 3;
        cell.mergeTableOptions(tableOptions);
        expect(cell.paddingLeft).to.equal(3);
      });

      it('if set will override tableOptions.style', function () {
        var cell = new Cell({style:{'padding-left':2}});
        cell.mergeTableOptions(defaultOptions());
        expect(cell.paddingLeft).to.equal(2);

        cell = new Cell({style:{paddingLeft:3}});
        cell.mergeTableOptions(defaultOptions());
        expect(cell.paddingLeft).to.equal(3);
      });
    });

    describe('style.padding-right', function () {
      it('if unset will be copied from tableOptions.style', function () {
        var cell = new Cell();
        cell.mergeTableOptions(defaultOptions());
        expect(cell.paddingRight).to.equal(1);

        cell = new Cell();
        var tableOptions = defaultOptions();
        tableOptions.style['padding-right'] = 2;
        cell.mergeTableOptions(tableOptions);
        expect(cell.paddingRight).to.equal(2);

        cell = new Cell();
        tableOptions = defaultOptions();
        tableOptions.style.paddingRight = 3;
        cell.mergeTableOptions(tableOptions);
        expect(cell.paddingRight).to.equal(3);
      });

      it('if set will override tableOptions.style', function () {
        var cell = new Cell({style:{'padding-right':2}});
        cell.mergeTableOptions(defaultOptions());
        expect(cell.paddingRight).to.equal(2);

        cell = new Cell({style:{paddingRight:3}});
        cell.mergeTableOptions(defaultOptions());
        expect(cell.paddingRight).to.equal(3);
      });
    });

    describe('desiredWidth',function(){
      it('content(hello) padding(1,1) == 7',function(){
        var cell = new Cell('hello');
        cell.mergeTableOptions(defaultOptions());
        expect(cell.desiredWidth).to.equal(7);
      });

      it('content(hi) padding(1,2) == 5',function(){
        var cell = new Cell({content:'hi',style:{paddingRight:2}});
        var tableOptions = defaultOptions();
        cell.mergeTableOptions(tableOptions);
        expect(cell.desiredWidth).to.equal(5);
      });

      it('content(hi) padding(3,2) == 7',function(){
        var cell = new Cell({content:'hi',style:{paddingLeft:3,paddingRight:2}});
        var tableOptions = defaultOptions();
        cell.mergeTableOptions(tableOptions);
        expect(cell.desiredWidth).to.equal(7);
      });
    });

    describe('desiredHeight',function(){
      it('1 lines of text',function(){
        var cell = new Cell('hi');
        cell.mergeTableOptions(defaultOptions());
        expect(cell.desiredHeight).to.equal(1);
      });

      it('2 lines of text',function(){
        var cell = new Cell('hi\nbye');
        cell.mergeTableOptions(defaultOptions());
        expect(cell.desiredHeight).to.equal(2);
      });

      it('2 lines of text',function(){
        var cell = new Cell('hi\nbye\nyo');
        cell.mergeTableOptions(defaultOptions());
        expect(cell.desiredHeight).to.equal(3);
      });
    });
  });

  describe('init',function(){
    describe('hAlign',function(){
      it('if unset takes colAlign value from tableOptions',function(){
        var tableOptions = defaultOptions();
        tableOptions.colAligns = ['left','right','both'];
        var cell = new Cell();
        cell.x = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.hAlign).to.equal('left');
        cell = new Cell();
        cell.x = 1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.hAlign).to.equal('right');
        cell = new Cell();
        cell.mergeTableOptions(tableOptions);
        cell.x=2;
        cell.init(tableOptions);
        expect(cell.hAlign).to.equal('both');
      });

      it('if set overrides tableOptions',function(){
        var tableOptions = defaultOptions();
        tableOptions.colAligns = ['left','right','both'];
        var cell = new Cell({hAlign:'right'});
        cell.x = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.hAlign).to.equal('right');
        cell = new Cell({hAlign:'left'});
        cell.x = 1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.hAlign).to.equal('left');
        cell = new Cell({hAlign:'right'});
        cell.x = 2;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.hAlign).to.equal('right');
      });
    });

    describe('vAlign',function(){
      it('if unset takes rowAlign value from tableOptions',function(){
        var tableOptions = defaultOptions();
        tableOptions.rowAligns = ['top','bottom','center'];
        var cell = new Cell();
        cell.y=0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.vAlign).to.equal('top');
        cell = new Cell();
        cell.y = 1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.vAlign).to.equal('bottom');
        cell = new Cell();
        cell.y = 2;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.vAlign).to.equal('center');
      });

      it('if set overrides tableOptions',function(){
        var tableOptions = defaultOptions();
        tableOptions.rowAligns = ['top','bottom','center'];

        var cell = new Cell({vAlign:'bottom'});
        cell.y = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.vAlign).to.equal('bottom');

        cell = new Cell({vAlign:'top'});
        cell.y = 1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.vAlign).to.equal('top');

        cell = new Cell({vAlign:'center'});
        cell.y = 2;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.vAlign).to.equal('center');
      });
    });

    describe('width', function(){
      it('will match colWidth of x',function(){
        var tableOptions = defaultOptions();
        tableOptions.colWidths = [5,10,15];

        var cell = new Cell();
        cell.x = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.width).to.equal(5);

        cell = new Cell();
        cell.x = 1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.width).to.equal(10);

        cell = new Cell();
        cell.x = 2;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.width).to.equal(15);
      });

      it('will add colWidths if colSpan > 1',function(){
        var tableOptions = defaultOptions();
        tableOptions.colWidths = [5,10,15];

        var cell = new Cell({colSpan:2});
        cell.x=0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.width).to.equal(16);

        cell = new Cell({colSpan:2});
        cell.x=1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.width).to.equal(26);

        cell = new Cell({colSpan:3});
        cell.x=0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.width).to.equal(32);
      });
    });

    describe('height', function(){
      it('will match rowHeight of x',function(){
        var tableOptions = defaultOptions();
        tableOptions.rowHeights = [5,10,15];

        var cell = new Cell();
        cell.y=0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.height).to.equal(5);

        cell = new Cell();
        cell.y=1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.height).to.equal(10);

        cell = new Cell();
        cell.y=2;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.height).to.equal(15);
      });

      it('will add rowHeights if rowSpan > 1',function(){
        var tableOptions = defaultOptions();
        tableOptions.rowHeights = [5,10,15];

        var cell = new Cell({rowSpan:2});
        cell.y = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.height).to.equal(16);

        cell = new Cell({rowSpan:2});
        cell.y = 1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.height).to.equal(26);

        cell = new Cell({rowSpan:3});
        cell.y = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.height).to.equal(32);
      });
    });

    describe('drawRight', function(){
      var tableOptions;

      beforeEach(function(){
        tableOptions = defaultOptions();
        tableOptions.colWidths = [20,20,20];
      });

      it('col 1 of 3, with default colspan',function(){
        var cell = new Cell();
        cell.x = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.drawRight).to.equal(false);
      });

      it('col 2 of 3, with default colspan',function(){
        var cell = new Cell();
        cell.x = 1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.drawRight).to.equal(false);
      });

      it('col 3 of 3, with default colspan',function(){
        var cell = new Cell();
        cell.x = 2;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.drawRight).to.equal(true);
      });

      it('col 3 of 4, with default colspan',function(){
        var cell = new Cell();
        cell.x = 2;
        tableOptions.colWidths = [20,20,20,20];
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.drawRight).to.equal(false);
      });

      it('col 2 of 3, with colspan of 2',function(){
        var cell = new Cell({colSpan:2});
        cell.x=1;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.drawRight).to.equal(true);
      });

      it('col 1 of 3, with colspan of 3',function(){
        var cell = new Cell({colSpan:3});
        cell.x = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.drawRight).to.equal(true);
      });

      it('col 1 of 3, with colspan of 2',function(){
        var cell = new Cell({colSpan:2});
        cell.x = 0;
        cell.mergeTableOptions(tableOptions);
        cell.init(tableOptions);
        expect(cell.drawRight).to.equal(false);
      });
    });
  });

  describe('drawLine', function(){
    var cell;

    beforeEach(function () {
      cell = new Cell();

      //manually init
      cell.chars = defaultChars();
      cell.paddingLeft = cell.paddingRight = 1;
      cell.width = 7;
      cell.height = 3;
      cell.hAlign = 'center';
      cell.vAlign = 'center';
      cell.chars.left = 'L';
      cell.chars.right = 'R';
      cell.chars.middle = 'M';
      cell.content = 'hello\nhowdy\ngoodnight';
      cell.lines = cell.content.split('\n');
      cell.x = cell.y = 0;
    });

    describe('top line',function(){
      it('will draw the top left corner when x=0,y=0',function(){
        cell.x = cell.y = 0;
        expect(cell.draw('top')).to.equal('┌───────');
        cell.drawRight = true;
        expect(cell.draw('top')).to.equal('┌───────┐');
      });

      it('will draw the top mid corner when x=1,y=0',function(){
        cell.x = 1;
        cell.y = 0;
        expect(cell.draw('top')).to.equal('┬───────');
        cell.drawRight = true;
        expect(cell.draw('top')).to.equal('┬───────┐');
      });

      it('will draw the left mid corner when x=0,y=1',function(){
        cell.x = 0;
        cell.y = 1;
        expect(cell.draw('top')).to.equal('├───────');
        cell.drawRight = true;
        expect(cell.draw('top')).to.equal('├───────┤');
      });

      it('will draw the mid mid corner when x=1,y=1',function(){
        cell.x = 1;
        cell.y = 1;
        expect(cell.draw('top')).to.equal('┼───────');
        cell.drawRight = true;
        expect(cell.draw('top')).to.equal('┼───────┤');
      });

      it('will draw in the color specified by border style',function(){
        cell.border = ['gray'];
        expect(cell.draw('top')).to.equal(colors.gray('┌───────'))
      });
    });

    describe('bottom line',function(){
      it('will draw the bottom left corner if x=0',function(){
        cell.x = 0;
        cell.y = 1;
        expect(cell.draw('bottom')).to.equal('└───────');
        cell.drawRight = true;
        expect(cell.draw('bottom')).to.equal('└───────┘');
      });

      it('will draw the bottom left corner if x=1',function(){
        cell.x = 1;
        cell.y = 1;
        expect(cell.draw('bottom')).to.equal('┴───────');
        cell.drawRight = true;
        expect(cell.draw('bottom')).to.equal('┴───────┘');
      });

      it('will draw in the color specified by border style',function(){
        cell.border = ['gray'];
        expect(cell.draw('bottom')).to.equal(colors.gray('└───────'))
      });
    });

    describe('drawBottom',function(){
      it('draws an empty line',function(){
        expect(cell.drawEmpty()).to.equal('L       ');
        expect(cell.drawEmpty(true)).to.equal('L       R');
      });

      it('draws an empty line',function(){
        cell.border = ['gray'];
        cell.head = ['red'];
        expect(cell.drawEmpty()).to.equal(colors.gray('L') + colors.red('       '));
        expect(cell.drawEmpty(true)).to.equal(colors.gray('L') + colors.red('       ') + colors.gray('R'));
      });
    });

    describe('first line of text',function(){
      beforeEach(function () {
        cell.width = 9;
      });

      it('will draw left side if x=0',function(){
        cell.x = 0;
        expect(cell.draw(0)).to.equal('L  hello  ');
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('L  hello  R');
      });

      it('will draw mid side if x=1',function(){
        cell.x = 1;
        expect(cell.draw(0)).to.equal('M  hello  ');
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('M  hello  R');
      });

      it('will align left',function(){
        cell.x = 1;
        cell.hAlign = 'left';
        expect(cell.draw(0)).to.equal('M hello   ');
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('M hello   R');
      });

      it('will align right',function(){
        cell.x = 1;
        cell.hAlign = 'right';
        expect(cell.draw(0)).to.equal('M   hello ');
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('M   hello R');
      });

      it('left and right will be drawn in color of border style',function(){
        cell.border = ['gray'];
        cell.x = 0;
        expect(cell.draw(0)).to.equal(colors.gray('L') + '  hello  ');
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal(colors.gray('L') + '  hello  ' + colors.gray('R'));
      });

      it('text will be drawn in color of head style if y == 0',function(){
        cell.head = ['red'];
        cell.x = cell.y = 0;
        expect(cell.draw(0)).to.equal('L' + colors.red('  hello  '));
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('L' + colors.red('  hello  ') + 'R');
      });

      it('text will NOT be drawn in color of head style if y == 1',function(){
        cell.head = ['red'];
        cell.x = cell.y = 1;
        expect(cell.draw(0)).to.equal('M  hello  ');
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('M  hello  R');
      });

      it('head and border colors together',function(){
        cell.border = ['gray'];
        cell.head = ['red'];
        cell.x = cell.y = 0;
        expect(cell.draw(0)).to.equal(colors.gray('L') + colors.red('  hello  '));
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal(colors.gray('L') + colors.red('  hello  ') + colors.gray('R'));
      });
    });    
    
    describe('second line of text',function(){
      beforeEach(function () {
        cell.width = 9;
      });

      it('will draw left side if x=0',function(){
        cell.x = 0;
        expect(cell.draw(1)).to.equal('L  howdy  ');
        cell.drawRight = true;
        expect(cell.draw(1)).to.equal('L  howdy  R');
      });

      it('will draw mid side if x=1',function(){
        cell.x = 1;
        expect(cell.draw(1)).to.equal('M  howdy  ');
        cell.drawRight = true;
        expect(cell.draw(1)).to.equal('M  howdy  R');
      });

      it('will align left',function(){
        cell.x = 1;
        cell.hAlign = 'left';
        expect(cell.draw(1)).to.equal('M howdy   ');
        cell.drawRight = true;
        expect(cell.draw(1)).to.equal('M howdy   R');
      });

      it('will align right',function(){
        cell.x = 1;
        cell.hAlign = 'right';
        expect(cell.draw(1)).to.equal('M   howdy ');
        cell.drawRight = true;
        expect(cell.draw(1)).to.equal('M   howdy R');
      });
    });    
    
    describe('truncated line of text',function(){
      beforeEach(function () {
        cell.width = 9;
      });

      it('will draw left side if x=0',function(){
        cell.x = 0;
        expect(cell.draw(2)).to.equal('L goodni… ');
        cell.drawRight = true;
        expect(cell.draw(2)).to.equal('L goodni… R');
      });

      it('will draw mid side if x=1',function(){
        cell.x = 1;
        expect(cell.draw(2)).to.equal('M goodni… ');
        cell.drawRight = true;
        expect(cell.draw(2)).to.equal('M goodni… R');
      });

      it('will not change when aligned left',function(){
        cell.x = 1;
        cell.hAlign = 'left';
        expect(cell.draw(2)).to.equal('M goodni… ');
        cell.drawRight = true;
        expect(cell.draw(2)).to.equal('M goodni… R');
      });

      it('will not change when aligned right',function(){
        cell.x = 1;
        cell.hAlign = 'right';
        expect(cell.draw(2)).to.equal('M goodni… ');
        cell.drawRight = true;
        expect(cell.draw(2)).to.equal('M goodni… R');
      });
    });

    describe('vAlign',function(){
      beforeEach(function () {
        cell.height = '5';
      });

      it('center',function(){
        cell.vAlign = 'center';
        expect(cell.draw(0)).to.equal('L       ');
        expect(cell.draw(1)).to.equal('L hello ');
        expect(cell.draw(2)).to.equal('L howdy ');
        expect(cell.draw(3)).to.equal('L good… ');
        expect(cell.draw(4)).to.equal('L       ');

        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('L       R');
        expect(cell.draw(1)).to.equal('L hello R');
        expect(cell.draw(2)).to.equal('L howdy R');
        expect(cell.draw(3)).to.equal('L good… R');
        expect(cell.draw(4)).to.equal('L       R');

        cell.x = 1;
        cell.drawRight = false;
        expect(cell.draw(0)).to.equal('M       ');
        expect(cell.draw(1)).to.equal('M hello ');
        expect(cell.draw(2)).to.equal('M howdy ');
        expect(cell.draw(3)).to.equal('M good… ');
        expect(cell.draw(4)).to.equal('M       ');
      });

      it('top',function(){
        cell.vAlign = 'top';
        expect(cell.draw(0)).to.equal('L hello ');
        expect(cell.draw(1)).to.equal('L howdy ');
        expect(cell.draw(2)).to.equal('L good… ');
        expect(cell.draw(3)).to.equal('L       ');
        expect(cell.draw(4)).to.equal('L       ');

        cell.vAlign = null; //top is the default
        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('L hello R');
        expect(cell.draw(1)).to.equal('L howdy R');
        expect(cell.draw(2)).to.equal('L good… R');
        expect(cell.draw(3)).to.equal('L       R');
        expect(cell.draw(4)).to.equal('L       R');

        cell.x = 1;
        cell.drawRight = false;
        expect(cell.draw(0)).to.equal('M hello ');
        expect(cell.draw(1)).to.equal('M howdy ');
        expect(cell.draw(2)).to.equal('M good… ');
        expect(cell.draw(3)).to.equal('M       ');
        expect(cell.draw(4)).to.equal('M       ');
      });

      it('center',function(){
        cell.vAlign = 'bottom';
        expect(cell.draw(0)).to.equal('L       ');
        expect(cell.draw(1)).to.equal('L       ');
        expect(cell.draw(2)).to.equal('L hello ');
        expect(cell.draw(3)).to.equal('L howdy ');
        expect(cell.draw(4)).to.equal('L good… ');

        cell.drawRight = true;
        expect(cell.draw(0)).to.equal('L       R');
        expect(cell.draw(1)).to.equal('L       R');
        expect(cell.draw(2)).to.equal('L hello R');
        expect(cell.draw(3)).to.equal('L howdy R');
        expect(cell.draw(4)).to.equal('L good… R');

        cell.x = 1;
        cell.drawRight = false;
        expect(cell.draw(0)).to.equal('M       ');
        expect(cell.draw(1)).to.equal('M       ');
        expect(cell.draw(2)).to.equal('M hello ');
        expect(cell.draw(3)).to.equal('M howdy ');
        expect(cell.draw(4)).to.equal('M good… ');
      });
    });

    it('vertically truncated will show truncation on last visible line',function(){
      cell.height = 2;
      expect(cell.draw(0)).to.equal('L hello ');
      expect(cell.draw(1)).to.equal('L howd… ');
    });

    it("won't vertically truncate if the lines just fit",function(){
      cell.height = 2;
      cell.content = "hello\nhowdy";
      cell.lines = cell.content.split("\n");
      expect(cell.draw(0)).to.equal('L hello ');
      expect(cell.draw(1)).to.equal('L howdy ');
    });

    it("will vertically truncate even if last line is short",function(){
      cell.height = 2;
      cell.content = "hello\nhi\nhowdy";
      cell.lines = cell.content.split("\n");
      expect(cell.draw(0)).to.equal('L hello ');
      expect(cell.draw(1)).to.equal('L  hi…  ');
    });

    it("allows custom truncation",function(){
      cell.height = 2;
      cell.truncate = '...';
      cell.content = "hello\nhi\nhowdy";
      cell.lines = cell.content.split("\n");
      expect(cell.draw(0)).to.equal('L hello ');
      expect(cell.draw(1)).to.equal('L hi... ');

      cell.content = "hello\nhowdy\nhi";
      cell.lines = cell.content.split("\n");
      expect(cell.draw(0)).to.equal('L hello ');
      expect(cell.draw(1)).to.equal('L ho... ');
    });
  });

  describe("ColSpanCell",function(){
    it('has an init function',function(){
      expect(new ColSpanCell()).to.respondTo('init');
      new ColSpanCell().init(); // nothing happens.
    });

    it('draw returns an empty string',function(){
      expect(new ColSpanCell().draw('top')).to.equal('');
      expect(new ColSpanCell().draw('bottom')).to.equal('');
      expect(new ColSpanCell().draw(1)).to.equal('');
    });
  });

  describe("RowSpanCell",function(){
    var original, tableOptions;

    beforeEach(function () {
      original = {
        rowSpan:3,
        y:0,
        draw:sinon.spy()
      };
      tableOptions = {
        rowHeights:[2,3,4,5]
      }
    });

    it('drawing top of the next row',function(){
      var spanner = new RowSpanCell(original);
      spanner.x = 0;
      spanner.y = 1;
      spanner.init(tableOptions);
      spanner.draw('top');
      expect(original.draw).to.have.been.calledOnce.and.calledWith(2);
    });

    it('drawing line 0 of the next row',function(){
      var spanner = new RowSpanCell(original);
      spanner.x = 0;
      spanner.y = 1;
      spanner.init(tableOptions);
      spanner.draw(0);
      expect(original.draw).to.have.been.calledOnce.and.calledWith(3);
    });

    it('drawing line 1 of the next row',function(){
      var spanner = new RowSpanCell(original);
      spanner.x  = 0;
      spanner.y = 1;
      spanner.init(tableOptions);
      spanner.draw(1);
      expect(original.draw).to.have.been.calledOnce.and.calledWith(4);
    });

    it('drawing top of two rows below',function(){
      var spanner = new RowSpanCell(original);
      spanner.x = 0;
      spanner.y = 2;
      spanner.init(tableOptions);
      spanner.draw('top');
      expect(original.draw).to.have.been.calledOnce.and.calledWith(6);
    });

    it('drawing line 0 of two rows below',function(){
      var spanner = new RowSpanCell(original);
      spanner.x = 0;
      spanner.y = 2;
      spanner.init(tableOptions);
      spanner.draw(0);
      expect(original.draw).to.have.been.calledOnce.and.calledWith(7);
    });

    it('drawing line 1 of two rows below',function(){
      var spanner = new RowSpanCell(original);
      spanner.x = 0;
      spanner.y = 2;
      spanner.init(tableOptions);
      spanner.draw(1);
      expect(original.draw).to.have.been.calledOnce.and.calledWith(8);
    });

    it('drawing bottom',function(){
      var spanner = new RowSpanCell(original);
      spanner.x = 0;
      spanner.y = 1;
      spanner.init(tableOptions);
      spanner.draw('bottom');
      expect(original.draw).to.have.been.calledOnce.and.calledWith('bottom');
    });
  });
});

