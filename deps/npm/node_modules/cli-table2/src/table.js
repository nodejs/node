
var utils = require('./utils');
var tableLayout = require('./layout-manager');
var _ = require('lodash');

function Table(options){
  this.options = utils.mergeOptions(options);
}

Table.prototype.__proto__ = Array.prototype;

Table.prototype.toString = function(){
  var array = this;
  var headersPresent = this.options.head && this.options.head.length;
  if(headersPresent){
    array = [this.options.head];
    if(this.length){
      array.push.apply(array,this);
    }
  }
  else {
    this.options.style.head=[];
  }

  var cells = tableLayout.makeTableLayout(array);

  _.forEach(cells,function(row){
    _.forEach(row,function(cell){
      cell.mergeTableOptions(this.options,cells);
    },this);
  },this);

  tableLayout.computeWidths(this.options.colWidths,cells);
  tableLayout.computeHeights(this.options.rowHeights,cells);

  _.forEach(cells,function(row,rowIndex){
    _.forEach(row,function(cell,cellIndex){
      cell.init(this.options);
    },this);
  },this);

  var result = [];

  for(var rowIndex = 0; rowIndex < cells.length; rowIndex++){
    var row = cells[rowIndex];
    var heightOfRow = this.options.rowHeights[rowIndex];

    if(rowIndex === 0 || !this.options.style.compact || (rowIndex == 1 && headersPresent)){
      doDraw(row,'top',result);
    }

    for(var lineNum = 0; lineNum < heightOfRow; lineNum++){
      doDraw(row,lineNum,result);
    }

    if(rowIndex + 1 == cells.length){
      doDraw(row,'bottom',result);
    }
  }

  return result.join('\n');
};

function doDraw(row,lineNum,result){
  var line = [];
  _.forEach(row,function(cell){
    line.push(cell.draw(lineNum));
  });
  var str = line.join('');
  if(str.length) result.push(str);
}

Table.prototype.__defineGetter__('width', function (){
  var str = this.toString().split("\n");
  return str[0].length;
});

module.exports = Table;