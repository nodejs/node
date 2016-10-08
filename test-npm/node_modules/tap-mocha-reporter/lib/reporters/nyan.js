/**
 * Module dependencies.
 */

var Base = require('./base');

/**
 * Expose `Dot`.
 */

exports = module.exports = NyanCat;

/**
 * Initialize a new `Dot` matrix test reporter.
 *
 * @param {Runner} runner
 * @api public
 */

function NyanCat(runner) {
  Base.call(this, runner);
  var self = this
    , stats = this.stats
    , width = Base.window.width * .75 | 0
    , rainbowColors = this.rainbowColors = self.generateColors()
    , colorIndex = this.colorIndex = 0
    , numerOfLines = this.numberOfLines = 4
    , trajectories = this.trajectories = [[], [], [], []]
    , nyanCatWidth = this.nyanCatWidth = 11
    , trajectoryWidthMax = this.trajectoryWidthMax = (width - nyanCatWidth)
    , scoreboardWidth = this.scoreboardWidth = 5
    , tick = this.tick = 0
    , n = 0;

  runner.on('start', function(){
    Base.cursor.hide();
    self.draw();
  });

  runner.on('pending', function(test){
    self.draw();
  });

  runner.on('pass', function(test){
    self.draw();
  });

  runner.on('fail', function(test, err){
    self.draw();
  });

  runner.on('end', function(){
    Base.cursor.show();
    for (var i = 0; i < self.numberOfLines; i++) write('\n');
    self.epilogue();
  });
}

/**
 * Draw the nyan cat
 *
 * @api private
 */

NyanCat.prototype.draw = function(){
  this.appendRainbow();
  this.drawScoreboard();
  this.drawRainbow();
  this.drawNyanCat();
  this.tick = !this.tick;
};

/**
 * Draw the "scoreboard" showing the number
 * of passes, failures and pending tests.
 *
 * @api private
 */

NyanCat.prototype.drawScoreboard = function(){
  var stats = this.stats;

  function draw(type, n) {
    write(' ');
    write(Base.color(type, n));
    write('\n');
  }

  draw('green', stats.passes);
  draw('fail', stats.failures);
  draw('pending', stats.pending);
  write('\n');

  this.cursorUp(this.numberOfLines);
};

/**
 * Append the rainbow.
 *
 * @api private
 */

NyanCat.prototype.appendRainbow = function(){
  var segment = this.tick ? '_' : '-';
  var rainbowified = this.rainbowify(segment);

  for (var index = 0; index < this.numberOfLines; index++) {
    var trajectory = this.trajectories[index];
    if (trajectory.length >= this.trajectoryWidthMax) trajectory.shift();
    trajectory.push(rainbowified);
  }
};

/**
 * Draw the rainbow.
 *
 * @api private
 */

NyanCat.prototype.drawRainbow = function(){
  var self = this;

  this.trajectories.forEach(function(line, index) {
    write('\u001b[' + self.scoreboardWidth + 'C');
    write(line.join(''));
    write('\n');
  });

  this.cursorUp(this.numberOfLines);
};

/**
 * Draw the nyan cat
 *
 * @api private
 */

NyanCat.prototype.drawNyanCat = function() {
  var self = this;
  var startWidth = this.scoreboardWidth + this.trajectories[0].length;
  var dist = '\u001b[' + startWidth + 'C';
  var padding = '';

  write(dist);
  write('_,------,');
  write('\n');

  write(dist);
  padding = self.tick ? '  ' : '   ';
  write('_|' + padding + '/\\_/\\ ');
  write('\n');

  write(dist);
  padding = self.tick ? '_' : '__';
  var tail = self.tick ? '~' : '^';
  var face;
  write(tail + '|' + padding + this.face() + ' ');
  write('\n');

  write(dist);
  padding = self.tick ? ' ' : '  ';
  write(padding + '""  "" ');
  write('\n');

  this.cursorUp(this.numberOfLines);
};

/**
 * Draw nyan cat face.
 *
 * @return {String}
 * @api private
 */

NyanCat.prototype.face = function() {
  var stats = this.stats;
  if (stats.failures) {
    return '( x .x)';
  } else if (stats.pending) {
    return '( o .o)';
  } else if(stats.passes) {
    return '( ^ .^)';
  } else {
    return '( - .-)';
  }
};

/**
 * Move cursor up `n`.
 *
 * @param {Number} n
 * @api private
 */

NyanCat.prototype.cursorUp = function(n) {
  write('\u001b[' + n + 'A');
};

/**
 * Move cursor down `n`.
 *
 * @param {Number} n
 * @api private
 */

NyanCat.prototype.cursorDown = function(n) {
  write('\u001b[' + n + 'B');
};

/**
 * Generate rainbow colors.
 *
 * @return {Array}
 * @api private
 */

NyanCat.prototype.generateColors = function(){
  var colors = [];

  for (var i = 0; i < (6 * 7); i++) {
    var pi3 = Math.floor(Math.PI / 3);
    var n = (i * (1.0 / 6));
    var r = Math.floor(3 * Math.sin(n) + 3);
    var g = Math.floor(3 * Math.sin(n + 2 * pi3) + 3);
    var b = Math.floor(3 * Math.sin(n + 4 * pi3) + 3);
    colors.push(36 * r + 6 * g + b + 16);
  }

  return colors;
};

/**
 * Apply rainbow to the given `str`.
 *
 * @param {String} str
 * @return {String}
 * @api private
 */

NyanCat.prototype.rainbowify = function(str){
  if (!Base.useColors)
    return str;
  var color = this.rainbowColors[this.colorIndex % this.rainbowColors.length];
  this.colorIndex += 1;
  return '\u001b[38;5;' + color + 'm' + str + '\u001b[0m';
};

/**
 * Stdout helper.
 */

function write(string) {
  process.stdout.write(string);
}

/**
 * Inherit from `Base.prototype`.
 */

NyanCat.prototype.__proto__ = Base.prototype;
