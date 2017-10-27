var _ = require('lodash');
var utils = require('./utils');

/**
 * A representation of a cell within the table.
 * Implementations must have `init` and `draw` methods,
 * as well as `colSpan`, `rowSpan`, `desiredHeight` and `desiredWidth` properties.
 * @param options
 * @constructor
 */
function Cell(options){
  this.setOptions(options);
}

Cell.prototype.setOptions = function(options){
  if(_.isString(options) || _.isNumber(options) || _.isBoolean(options)){
    options = {content:''+options};
  }
  options = options || {};
  this.options = options;
  var content = options.content;
  if (_.isString(content) || _.isNumber(content) || _.isBoolean(content)) {
    this.content = String(content);
  } else if (!content) {
    this.content = '';
  } else {
    throw new Error('Content needs to be a primitive, got: ' + (typeof  content));
  }
  this.colSpan = options.colSpan || 1;
  this.rowSpan = options.rowSpan || 1;
};

Cell.prototype.mergeTableOptions = function(tableOptions,cells){
  this.cells = cells;

  var optionsChars = this.options.chars || {};
  var tableChars = tableOptions.chars;
  var chars = this.chars = {};
  _.forEach(CHAR_NAMES,function(name){
     setOption(optionsChars,tableChars,name,chars);
  });

  this.truncate = this.options.truncate || tableOptions.truncate;

  var style = this.options.style = this.options.style || {};
  var tableStyle = tableOptions.style;
  setOption(style, tableStyle, 'padding-left', this);
  setOption(style, tableStyle, 'padding-right', this);
  this.head = style.head || tableStyle.head;
  this.border = style.border || tableStyle.border;

  var fixedWidth = tableOptions.colWidths[this.x];
  if(tableOptions.wordWrap && fixedWidth){
    fixedWidth -= this.paddingLeft + this.paddingRight;
    this.lines = utils.colorizeLines(utils.wordWrap(fixedWidth,this.content));
  }
  else {
    this.lines = utils.colorizeLines(this.content.split('\n'));
  }

  this.desiredWidth = utils.strlen(this.content) + this.paddingLeft + this.paddingRight;
  this.desiredHeight = this.lines.length;
};

/**
 * Each cell will have it's `x` and `y` values set by the `layout-manager` prior to
 * `init` being called;
 * @type {Number}
 */

Cell.prototype.x = null;
Cell.prototype.y = null;

/**
 * Initializes the Cells data structure.
 *
 * @param tableOptions - A fully populated set of tableOptions.
 * In addition to the standard default values, tableOptions must have fully populated the
 * `colWidths` and `rowWidths` arrays. Those arrays must have lengths equal to the number
 * of columns or rows (respectively) in this table, and each array item must be a Number.
 *
 */
Cell.prototype.init = function(tableOptions){
  var x = this.x;
  var y = this.y;
  this.widths = tableOptions.colWidths.slice(x, x + this.colSpan);
  this.heights = tableOptions.rowHeights.slice(y, y + this.rowSpan);
  this.width = _.reduce(this.widths,sumPlusOne);
  this.height = _.reduce(this.heights,sumPlusOne);

  this.hAlign = this.options.hAlign || tableOptions.colAligns[x];
  this.vAlign = this.options.vAlign || tableOptions.rowAligns[y];

  this.drawRight = x + this.colSpan == tableOptions.colWidths.length;
};

/**
 * Draws the given line of the cell.
 * This default implementation defers to methods `drawTop`, `drawBottom`, `drawLine` and `drawEmpty`.
 * @param lineNum - can be `top`, `bottom` or a numerical line number.
 * @param spanningCell - will be a number if being called from a RowSpanCell, and will represent how
 * many rows below it's being called from. Otherwise it's undefined.
 * @returns {String} The representation of this line.
 */
Cell.prototype.draw = function(lineNum,spanningCell){
  if(lineNum == 'top') return this.drawTop(this.drawRight);
  if(lineNum == 'bottom') return this.drawBottom(this.drawRight);
  var padLen = Math.max(this.height - this.lines.length, 0);
  var padTop;
  switch (this.vAlign){
    case 'center':
      padTop = Math.ceil(padLen / 2);
      break;
    case 'bottom':
      padTop = padLen;
      break;
    default :
      padTop = 0;
  }
  if( (lineNum < padTop) || (lineNum >= (padTop + this.lines.length))){
    return this.drawEmpty(this.drawRight,spanningCell);
  }
  var forceTruncation = (this.lines.length > this.height) && (lineNum + 1 >= this.height);
  return this.drawLine(lineNum - padTop, this.drawRight, forceTruncation,spanningCell);
};

/**
 * Renders the top line of the cell.
 * @param drawRight - true if this method should render the right edge of the cell.
 * @returns {String}
 */
Cell.prototype.drawTop = function(drawRight){
  var content = [];
  if(this.cells){  //TODO: cells should always exist - some tests don't fill it in though
    _.forEach(this.widths,function(width,index){
      content.push(this._topLeftChar(index));
      content.push(
        utils.repeat(this.chars[this.y == 0 ? 'top' : 'mid'],width)
      );
    },this);
  }
  else {
    content.push(this._topLeftChar(0));
    content.push(utils.repeat(this.chars[this.y == 0 ? 'top' : 'mid'],this.width));
  }
  if(drawRight){
    content.push(this.chars[this.y == 0 ? 'topRight' : 'rightMid']);
  }
  return this.wrapWithStyleColors('border',content.join(''));
};

Cell.prototype._topLeftChar = function(offset){
  var x = this.x+offset;
  var leftChar;
  if(this.y == 0){
    leftChar = x == 0 ? 'topLeft' : (offset == 0 ? 'topMid' : 'top');
  } else  {
    if(x == 0){
      leftChar = 'leftMid';
    }
    else {
      leftChar = offset == 0 ? 'midMid' : 'bottomMid';
      if(this.cells){  //TODO: cells should always exist - some tests don't fill it in though
        var spanAbove = this.cells[this.y-1][x] instanceof Cell.ColSpanCell;
        if(spanAbove){
          leftChar = offset == 0 ? 'topMid' : 'mid';
        }
        if(offset == 0){
          var i = 1;
          while(this.cells[this.y][x-i] instanceof Cell.ColSpanCell){
            i++;
          }
          if(this.cells[this.y][x-i] instanceof Cell.RowSpanCell){
            leftChar = 'leftMid';
          }
        }
      }
    }
  }
  return this.chars[leftChar];
};

Cell.prototype.wrapWithStyleColors = function(styleProperty,content){
  if(this[styleProperty] && this[styleProperty].length){
    try {
      var colors = require('colors/safe');
      for(var i = this[styleProperty].length - 1; i >= 0; i--){
        colors = colors[this[styleProperty][i]];
      }
      return colors(content);
    } catch (e) {
      return content;
    }
  }
  else {
    return content;
  }
};

/**
 * Renders a line of text.
 * @param lineNum - Which line of text to render. This is not necessarily the line within the cell.
 * There may be top-padding above the first line of text.
 * @param drawRight - true if this method should render the right edge of the cell.
 * @param forceTruncationSymbol - `true` if the rendered text should end with the truncation symbol even
 * if the text fits. This is used when the cell is vertically truncated. If `false` the text should
 * only include the truncation symbol if the text will not fit horizontally within the cell width.
 * @param spanningCell - a number of if being called from a RowSpanCell. (how many rows below). otherwise undefined.
 * @returns {String}
 */
Cell.prototype.drawLine = function(lineNum,drawRight,forceTruncationSymbol,spanningCell){
  var left = this.chars[this.x == 0 ? 'left' : 'middle'];
  if(this.x && spanningCell && this.cells){
    var cellLeft = this.cells[this.y+spanningCell][this.x-1];
    while(cellLeft instanceof ColSpanCell){
      cellLeft = this.cells[cellLeft.y][cellLeft.x-1];
    }
    if(!(cellLeft instanceof RowSpanCell)){
      left = this.chars['rightMid'];
    }
  }
  var leftPadding = utils.repeat(' ', this.paddingLeft);
  var right = (drawRight ? this.chars['right'] : '');
  var rightPadding = utils.repeat(' ', this.paddingRight);
  var line = this.lines[lineNum];
  var len = this.width - (this.paddingLeft + this.paddingRight);
  if(forceTruncationSymbol) line += this.truncate || '…';
  var content = utils.truncate(line,len,this.truncate);
  content = utils.pad(content, len, ' ', this.hAlign);
  content = leftPadding + content + rightPadding;
  return this.stylizeLine(left,content,right);
};

Cell.prototype.stylizeLine = function(left,content,right){
  left = this.wrapWithStyleColors('border',left);
  right = this.wrapWithStyleColors('border',right);
  if(this.y === 0){
    content = this.wrapWithStyleColors('head',content);
  }
  return left + content + right;
};

/**
 * Renders the bottom line of the cell.
 * @param drawRight - true if this method should render the right edge of the cell.
 * @returns {String}
 */
Cell.prototype.drawBottom = function(drawRight){
  var left = this.chars[this.x == 0 ? 'bottomLeft' : 'bottomMid'];
  var content = utils.repeat(this.chars.bottom,this.width);
  var right = drawRight ? this.chars['bottomRight'] : '';
  return this.wrapWithStyleColors('border',left + content + right);
};

/**
 * Renders a blank line of text within the cell. Used for top and/or bottom padding.
 * @param drawRight - true if this method should render the right edge of the cell.
 * @param spanningCell - a number of if being called from a RowSpanCell. (how many rows below). otherwise undefined.
 * @returns {String}
 */
Cell.prototype.drawEmpty = function(drawRight,spanningCell){
  var left = this.chars[this.x == 0 ? 'left' : 'middle'];
  if(this.x && spanningCell && this.cells){
    var cellLeft = this.cells[this.y+spanningCell][this.x-1];
    while(cellLeft instanceof ColSpanCell){
      cellLeft = this.cells[cellLeft.y][cellLeft.x-1];
    }
    if(!(cellLeft instanceof RowSpanCell)){
      left = this.chars['rightMid'];
    }
  }
  var right = (drawRight ? this.chars['right'] : '');
  var content = utils.repeat(' ',this.width);
  return this.stylizeLine(left , content , right);
};

/**
 * A Cell that doesn't do anything. It just draws empty lines.
 * Used as a placeholder in column spanning.
 * @constructor
 */
function ColSpanCell(){}

ColSpanCell.prototype.draw = function(){
  return '';
};

ColSpanCell.prototype.init = function(tableOptions){};


/**
 * A placeholder Cell for a Cell that spans multiple rows.
 * It delegates rendering to the original cell, but adds the appropriate offset.
 * @param originalCell
 * @constructor
 */
function RowSpanCell(originalCell){
  this.originalCell = originalCell;
}

RowSpanCell.prototype.init = function(tableOptions){
  var y = this.y;
  var originalY = this.originalCell.y;
  this.cellOffset = y - originalY;
  this.offset = findDimension(tableOptions.rowHeights,originalY,this.cellOffset);
};

RowSpanCell.prototype.draw = function(lineNum){
  if(lineNum == 'top'){
    return this.originalCell.draw(this.offset,this.cellOffset);
  }
  if(lineNum == 'bottom'){
    return this.originalCell.draw('bottom');
  }
  return this.originalCell.draw(this.offset + 1 + lineNum);
};

ColSpanCell.prototype.mergeTableOptions =
RowSpanCell.prototype.mergeTableOptions = function(){};

// HELPER FUNCTIONS
function setOption(objA,objB,nameB,targetObj){
  var nameA = nameB.split('-');
  if(nameA.length > 1) {
    nameA[1] = nameA[1].charAt(0).toUpperCase() + nameA[1].substr(1);
    nameA = nameA.join('');
    targetObj[nameA] = objA[nameA] || objA[nameB] || objB[nameA] || objB[nameB];
  }
  else {
    targetObj[nameB] = objA[nameB] || objB[nameB];
  }
}

function findDimension(dimensionTable, startingIndex, span){
  var ret = dimensionTable[startingIndex];
  for(var i = 1; i < span; i++){
    ret += 1 + dimensionTable[startingIndex + i];
  }
  return ret;
}

function sumPlusOne(a,b){
  return a+b+1;
}

var CHAR_NAMES = [  'top'
  , 'top-mid'
  , 'top-left'
  , 'top-right'
  , 'bottom'
  , 'bottom-mid'
  , 'bottom-left'
  , 'bottom-right'
  , 'left'
  , 'left-mid'
  , 'mid'
  , 'mid-mid'
  , 'right'
  , 'right-mid'
  , 'middle'
];
module.exports = Cell;
module.exports.ColSpanCell = ColSpanCell;
module.exports.RowSpanCell = RowSpanCell;