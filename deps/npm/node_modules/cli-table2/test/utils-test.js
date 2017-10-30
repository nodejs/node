describe('utils',function(){
  var colors = require('colors/safe');
  var chai = require('chai');
  var utils = require('../src/utils');
  var expect = chai.expect;

  var strlen = utils.strlen;
  var repeat = utils.repeat;
  var pad = utils.pad;
  var truncate = utils.truncate;
  var mergeOptions = utils.mergeOptions;
  var wordWrap = utils.wordWrap;


  describe('strlen',function(){
    it('length of "hello" is 5',function(){
      expect(strlen('hello')).to.equal(5);
    });

    it('length of "hi" is 2',function(){
      expect(strlen('hi')).to.equal(2);
    });

    it('length of "hello" in red is 5',function(){
      expect(strlen(colors.red('hello'))).to.equal(5);
    });

    it('length of "hello" in zebra is 5',function(){
      expect(strlen(colors.zebra('hello'))).to.equal(5);
    });

    it('length of "hello\\nhi\\nheynow" is 6',function(){
      expect(strlen('hello\nhi\nheynow')).to.equal(6);
    });

    it('length of "中文字符" is 8',function(){
      expect(strlen('中文字符')).to.equal(8);
    });

    it('length of "日本語の文字" is 12',function(){
      expect(strlen('日本語の文字')).to.equal(12);
    });

    it('length of "한글" is 4',function(){
      expect(strlen('한글')).to.equal(4);
    });
  });

  describe('repeat',function(){
    it('"-" x 3',function(){
      expect(repeat('-',3)).to.equal('---');
    });

    it('"-" x 4',function(){
      expect(repeat('-',4)).to.equal('----');
    });

    it('"=" x 4',function(){
      expect(repeat('=',4)).to.equal('====');
    });
  });

  describe('pad',function(){
    it("pad('hello',6,' ', right) == ' hello'", function () {
      expect(pad('hello',6,' ','right')).to.equal(' hello');
    });

    it("pad('hello',7,' ', left) == 'hello  '", function () {
      expect(pad('hello',7,' ','left')).to.equal('hello  ');
    });

    it("pad('hello',8,' ', center) == ' hello  '", function () {
      expect(pad('hello',8,' ','center')).to.equal(' hello  ');
    });

    it("pad('hello',9,' ', center) == '  hello  '", function () {
      expect(pad('hello',9,' ','center')).to.equal('  hello  ');
    });

    it("pad('yo',4,' ', center) == ' yo '", function () {
      expect(pad('yo',4,' ','center')).to.equal(' yo ');
    });

    it('pad red(hello)', function(){
      expect(pad(colors.red('hello'),7,' ','right')).to.equal('  ' + colors.red('hello'));
    });

    it("pad('hello', 2, ' ', right) == 'hello'", function(){
      expect(pad('hello', 2, ' ', 'right')).to.equal('hello');
    });
  });

  describe('truncate',function(){
    it('truncate("hello", 5) === "hello"',function(){
      expect(truncate('hello',5)).to.equal('hello');
    });

    it('truncate("hello sir", 7, "…") == "hello …"',function(){
      expect(truncate('hello sir', 7, '…')).to.equal('hello …');
    });

    it('truncate("hello sir", 6, "…") == "hello…"',function(){
      expect(truncate('hello sir', 6, '…')).to.equal('hello…');
    });

    it('truncate("goodnight moon", 8, "…") == "goodnig…"',function(){
      expect(truncate('goodnight moon', 8, '…')).to.equal('goodnig…');
    });

    it('truncate(colors.zebra("goodnight moon"), 15, "…") == colors.zebra("goodnight moon")',function(){
      var original = colors.zebra('goodnight moon');
      expect(truncate(original, 15, '…')).to.equal(original);
    });

    it('truncate(colors.zebra("goodnight moon"), 8, "…") == colors.zebra("goodnig") + "…"',function(){
      var original = colors.zebra('goodnight moon');
      var expected = colors.zebra('goodnig') + '…';
      expect(truncate(original, 8, '…')).to.equal(expected);
    });

    it('truncate(colors.zebra("goodnight moon"), 9, "…") == colors.zebra("goodnig") + "…"',function(){
      var original = colors.zebra('goodnight moon');
      var expected = colors.zebra('goodnigh') + '…';
      expect(truncate(original, 9, '…')).to.equal(expected);
    });

    it('red(hello) + green(world) truncated to 9 chars',function(){
      var original = colors.red('hello') + colors.green(' world');
      var expected = colors.red('hello') + colors.green(' wo') + '…';
      expect(truncate(original, 9)).to.equal(expected);
    });

    it('red-on-green(hello) + green-on-red(world) truncated to 9 chars',function(){
      var original = colors.red.bgGreen('hello') + colors.green.bgRed(' world');
      var expected = colors.red.bgGreen('hello') + colors.green.bgRed(' wo') + '…';
      expect(truncate(original,9)).to.equal(expected);
    });

    it('red-on-green(hello) + green-on-red(world) truncated to 10 chars - using inverse',function(){
      var original = colors.red.bgGreen('hello' + colors.inverse(' world'));
      var expected = colors.red.bgGreen('hello' + colors.inverse(' wor')) + '…';
      expect(truncate(original,10)).to.equal(expected);
    });

    it('red-on-green( zebra (hello world) ) truncated to 11 chars',function(){
      var original = colors.red.bgGreen(colors.zebra('hello world'));
      var expected = colors.red.bgGreen(colors.zebra('hello world'));
      expect(truncate(original,11)).to.equal(expected);
    });

    it('red-on-green( zebra (hello world) ) truncated to 10 chars',function(){
      var original = colors.red.bgGreen(colors.zebra('hello world'));
      var expected = colors.red.bgGreen(colors.zebra('hello wor')) + '…';
      expect(truncate(original,10)).to.equal(expected);
    });

    it('handles reset code', function() {
      var original = '\x1b[31mhello\x1b[0m world';
      var expected = '\x1b[31mhello\x1b[0m wor…';
      expect(truncate(original,10)).to.equal(expected);
    });

    it('handles reset code (EMPTY VERSION)', function() {
      var original = '\x1b[31mhello\x1b[0m world';
      var expected = '\x1b[31mhello\x1b[0m wor…';
      expect(truncate(original,10)).to.equal(expected);
    });

    it('truncateWidth("漢字テスト", 15) === "漢字テスト"',function(){
      expect(truncate('漢字テスト',15)).to.equal('漢字テスト');
    });

    it('truncateWidth("漢字テスト", 6) === "漢字…"',function(){
      expect(truncate('漢字テスト',6)).to.equal('漢字…');
    });

    it('truncateWidth("漢字テスト", 5) === "漢字…"',function(){
      expect(truncate('漢字テスト',5)).to.equal('漢字…');
    });

    it('truncateWidth("漢字testてすと", 12) === "漢字testて…"',function(){
      expect(truncate('漢字testてすと',12)).to.equal('漢字testて…');
    });

    it('handles color code with CJK chars',function(){
      var original = '漢字\x1b[31m漢字\x1b[0m漢字';
      var expected = '漢字\x1b[31m漢字\x1b[0m漢…';
      expect(truncate(original,11)).to.equal(expected);
    });
  });

  function defaultOptions(){
    return {
      chars: {
        'top': '─'
        , 'top-mid': '┬'
        , 'top-left': '┌'
        , 'top-right': '┐'
        , 'bottom': '─'
        , 'bottom-mid': '┴'
        , 'bottom-left': '└'
        , 'bottom-right': '┘'
        , 'left': '│'
        , 'left-mid': '├'
        , 'mid': '─'
        , 'mid-mid': '┼'
        , 'right': '│'
        , 'right-mid': '┤'
        , 'middle': '│'
      }
      , truncate: '…'
      , colWidths: []
      , rowHeights: []
      , colAligns: []
      , rowAligns: []
      , style: {
        'padding-left': 1
        , 'padding-right': 1
        , head: ['red']
        , border: ['grey']
        , compact : false
      }
      , head: []
    };
  }

  describe('mergeOptions',function(){
    it('allows you to override chars',function(){
      expect(mergeOptions()).to.eql(defaultOptions());
    });

    it('chars will be merged deeply',function(){
      var expected = defaultOptions();
      expected.chars.left = 'L';
      expect(mergeOptions({chars:{left:'L'}})).to.eql(expected);
    });

    it('style will be merged deeply',function(){
      var expected = defaultOptions();
      expected.style['padding-left'] = 2;
      expect(mergeOptions({style:{'padding-left':2}})).to.eql(expected);
    });

    it('head will be overwritten',function(){
      var expected = defaultOptions();
      expected.style.head = [];
      //we can't use lodash's `merge()` in implementation because it would deeply copy array.
      expect(mergeOptions({style:{'head':[]}})).to.eql(expected);
    });

    it('border will be overwritten',function(){
      var expected = defaultOptions();
      expected.style.border = [];
      //we can't use lodash's `merge()` in implementation because it would deeply copy array.
      expect(mergeOptions({style:{'border':[]}})).to.eql(expected);
    });
  });

  describe('wordWrap',function(){
    it('length',function(){
      var input = 'Hello, how are you today? I am fine, thank you!';

      var expected = 'Hello, how\nare you\ntoday? I\nam fine,\nthank you!';

      expect(wordWrap(10,input).join('\n')).to.equal(expected);
    });

    it('length with colors',function(){
      var input = colors.red('Hello, how are') + colors.blue(' you today? I') + colors.green(' am fine, thank you!');

      var expected = colors.red('Hello, how\nare') + colors.blue(' you\ntoday? I') + colors.green('\nam fine,\nthank you!');

      expect(wordWrap(10,input).join('\n')).to.equal(expected);
    });

    it('will not create an empty last line',function(){
      var input = 'Hello Hello ';

      var expected = 'Hello\nHello';

      expect(wordWrap(5,input).join('\n')).to.equal(expected);
    });

    it('will handle color reset code',function(){
      var input = '\x1b[31mHello\x1b[0m Hello ';

      var expected = '\x1b[31mHello\x1b[0m\nHello';

      expect(wordWrap(5,input).join('\n')).to.equal(expected);
    });

    it('will handle color reset code (EMPTY version)',function(){
      var input = '\x1b[31mHello\x1b[m Hello ';

      var expected = '\x1b[31mHello\x1b[m\nHello';

      expect(wordWrap(5,input).join('\n')).to.equal(expected);
    });

    it('words longer than limit will not create extra newlines',function(){
      var input = 'disestablishment is a multiplicity someotherlongword';

      var expected = 'disestablishment\nis a\nmultiplicity\nsomeotherlongword';

      expect(wordWrap(7,input).join('\n')).to.equal(expected);
    });

    it('multiple line input',function(){
      var input = 'a\nb\nc d e d b duck\nm\nn\nr';
      var expected = ['a', 'b', 'c d', 'e d', 'b', 'duck', 'm', 'n', 'r'];

      expect(wordWrap(4,input)).to.eql(expected);
    });

    it('will not start a line with whitespace', function(){
      var input = 'ab cd  ef gh  ij kl';
      var expected = ['ab cd','ef gh', 'ij kl'];
      expect(wordWrap(7, input)).to.eql(expected);
    });

    it('wraps CJK chars', function(){
      var input = '漢字 漢\n字 漢字';
      var expected = ['漢字 漢','字 漢字'];
      expect(wordWrap(7, input)).to.eql(expected);
    });

    it('wraps CJK chars with colors', function(){
      var input = '\x1b[31m漢字\x1b[0m\n 漢字';
      var expected = ['\x1b[31m漢字\x1b[0m', ' 漢字'];
      expect(wordWrap(5, input)).to.eql(expected);
    });
  });

  describe('colorizeLines',function(){
    it('foreground colors continue on each line',function(){
      var input = colors.red('Hello\nHi').split('\n');

      expect(utils.colorizeLines(input)).to.eql([
        colors.red('Hello'),
        colors.red('Hi')
      ]);
    });

    it('foreground colors continue on each line',function(){
      var input = colors.bgRed('Hello\nHi').split('\n');

      expect(utils.colorizeLines(input)).to.eql([
        colors.bgRed('Hello'),
        colors.bgRed('Hi')
      ]);
    });

    it('styles will continue on each line',function(){
      var input = colors.underline('Hello\nHi').split('\n');

      expect(utils.colorizeLines(input)).to.eql([
        colors.underline('Hello'),
        colors.underline('Hi')
      ]);
    });

    it('styles that end before the break will not be applied to the next line',function(){
      var input = (colors.underline('Hello') +'\nHi').split('\n');

      expect(utils.colorizeLines(input)).to.eql([
        colors.underline('Hello'),
        'Hi'
      ]);
    });

    it('the reset code can be used to drop styles', function() {
      var input = '\x1b[31mHello\x1b[0m\nHi'.split('\n');
      expect(utils.colorizeLines(input)).to.eql([
        "\x1b[31mHello\x1b[0m",
        "Hi"
      ]);
    });

    it('handles aixterm 16-color foreground', function() {
      var input = '\x1b[90mHello\nHi\x1b[0m'.split('\n');
      expect(utils.colorizeLines(input)).to.eql([
        '\x1b[90mHello\x1b[39m',
        '\x1b[90mHi\x1b[0m'
      ]);
    });

    it('handles aixterm 16-color background', function() {
      var input = '\x1b[100mHello\nHi\x1b[m\nHowdy'.split('\n');
      expect(utils.colorizeLines(input)).to.eql([
        '\x1b[100mHello\x1b[49m',
        '\x1b[100mHi\x1b[m',
        'Howdy'
      ]);
    });

    it('handles aixterm 256-color foreground', function() {
      var input ='\x1b[48;5;8mHello\nHi\x1b[0m\nHowdy'.split('\n');
      expect(utils.colorizeLines(input)).to.eql([
        '\x1b[48;5;8mHello\x1b[49m',
        '\x1b[48;5;8mHi\x1b[0m',
        'Howdy'
      ]);
    });

    it('handles CJK chars',function(){
      var input = colors.red('漢字\nテスト').split('\n');

      expect(utils.colorizeLines(input)).to.eql([
        colors.red('漢字'),
        colors.red('テスト')
      ]);
    });
  });
});