var _ = require('lodash');
var Cell = require('./cell');
var RowSpanCell = Cell.RowSpanCell;
var ColSpanCell = Cell.ColSpanCell;

(function(){
  function layoutTable(table){
    _.forEach(table,function(row,rowIndex){
      _.forEach(row,function(cell,columnIndex){
        cell.y = rowIndex;
        cell.x = columnIndex;
        for(var y = rowIndex; y >= 0; y--){
          var row2 = table[y];
          var xMax = (y === rowIndex) ? columnIndex : row2.length;
          for(var x = 0; x < xMax; x++){
            var cell2 = row2[x];
            while(cellsConflict(cell,cell2)){
              cell.x++;
            }
          }
        }
      });
    });
  }

  function maxWidth(table) {
    var mw = 0;
    _.forEach(table, function (row) {
      _.forEach(row, function (cell) {
        mw = Math.max(mw,cell.x + (cell.colSpan || 1));
      });
    });
    return mw;
  }

  function maxHeight(table){
    return table.length;
  }

  function cellsConflict(cell1,cell2){
    var yMin1 = cell1.y;
    var yMax1 = cell1.y - 1 + (cell1.rowSpan || 1);
    var yMin2 = cell2.y;
    var yMax2 = cell2.y - 1 + (cell2.rowSpan || 1);
    var yConflict = !(yMin1 > yMax2 || yMin2 > yMax1);

    var xMin1= cell1.x;
    var xMax1 = cell1.x - 1 + (cell1.colSpan || 1);
    var xMin2= cell2.x;
    var xMax2 = cell2.x - 1 + (cell2.colSpan || 1);
    var xConflict = !(xMin1 > xMax2 || xMin2 > xMax1);

    return yConflict && xConflict;
  }

  function conflictExists(rows,x,y){
    var i_max = Math.min(rows.length-1,y);
    var cell = {x:x,y:y};
    for(var i = 0; i <= i_max; i++){
      var row = rows[i];
      for(var j = 0; j < row.length; j++){
        if(cellsConflict(cell,row[j])){
          return true;
        }
      }
    }
    return false;
  }

  function allBlank(rows,y,xMin,xMax){
    for(var x = xMin; x < xMax; x++){
      if(conflictExists(rows,x,y)){
        return false;
      }
    }
    return true;
  }

  function addRowSpanCells(table){
    _.forEach(table,function(row,rowIndex){
      _.forEach(row,function(cell){
        for(var i = 1; i < cell.rowSpan; i++){
          var rowSpanCell = new RowSpanCell(cell);
          rowSpanCell.x = cell.x;
          rowSpanCell.y = cell.y + i;
          rowSpanCell.colSpan = cell.colSpan;
          insertCell(rowSpanCell,table[rowIndex+i]);
        }
      });
    });
  }

  function addColSpanCells(cellRows){
    for(var rowIndex = cellRows.length-1; rowIndex >= 0; rowIndex--) {
      var cellColumns = cellRows[rowIndex];
      for (var columnIndex = 0; columnIndex < cellColumns.length; columnIndex++) {
        var cell = cellColumns[columnIndex];
        for (var k = 1; k < cell.colSpan; k++) {
          var colSpanCell = new ColSpanCell();
          colSpanCell.x = cell.x + k;
          colSpanCell.y = cell.y;
          cellColumns.splice(columnIndex + 1, 0, colSpanCell);
        }
      }
    }
  }

  function insertCell(cell,row){
    var x = 0;
    while(x < row.length && (row[x].x < cell.x)) {
      x++;
    }
    row.splice(x,0,cell);
  }

  function fillInTable(table){
    var h_max = maxHeight(table);
    var w_max = maxWidth(table);
    for(var y = 0; y < h_max; y++){
      for(var x = 0; x < w_max; x++){
        if(!conflictExists(table,x,y)){
          var opts = {x:x,y:y,colSpan:1,rowSpan:1};
          x++;
          while(x < w_max && !conflictExists(table,x,y)){
            opts.colSpan++;
            x++;
          }
          var y2 = y + 1;
          while(y2 < h_max && allBlank(table,y2,opts.x,opts.x+opts.colSpan)){
            opts.rowSpan++;
            y2++;
          }

          var cell = new Cell(opts);
          cell.x = opts.x;
          cell.y = opts.y;
          insertCell(cell,table[y]);
        }
      }
    }
  }

  function generateCells(rows){
    return _.map(rows,function(row){
      if(!_.isArray(row)){
        var key = Object.keys(row)[0];
        row = row[key];
        if(_.isArray(row)){
          row = row.slice();
          row.unshift(key);
        }
        else {
          row = [key,row];
        }
      }
      return _.map(row,function(cell){
        return new Cell(cell);
      });
    });
  }

  function makeTableLayout(rows){
    var cellRows = generateCells(rows);
    layoutTable(cellRows);
    fillInTable(cellRows);
    addRowSpanCells(cellRows);
    addColSpanCells(cellRows);
    return cellRows;
  }

  module.exports = {
    makeTableLayout: makeTableLayout,
    layoutTable: layoutTable,
    addRowSpanCells: addRowSpanCells,
    maxWidth:maxWidth,
    fillInTable:fillInTable,
    computeWidths:makeComputeWidths('colSpan','desiredWidth','x',1),
    computeHeights:makeComputeWidths('rowSpan','desiredHeight','y',1)
  };
})();

function makeComputeWidths(colSpan,desiredWidth,x,forcedMin){
  return function(vals,table){
    var result = [];
    var spanners = [];
    _.forEach(table,function(row){
      _.forEach(row,function(cell){
        if((cell[colSpan] || 1) > 1){
          spanners.push(cell);
        }
        else {
          result[cell[x]] = Math.max(result[cell[x]] || 0, cell[desiredWidth] || 0, forcedMin);
        }
      });
    });

    _.forEach(vals,function(val,index){
      if(_.isNumber(val)){
        result[index] = val;
      }
    });

    //_.forEach(spanners,function(cell){
    for(var k = spanners.length - 1; k >=0; k--){
      var cell = spanners[k];
      var span = cell[colSpan];
      var col = cell[x];
      var existingWidth = result[col];
      var editableCols = _.isNumber(vals[col]) ? 0 : 1;
      for(var i = 1; i < span; i ++){
        existingWidth += 1 + result[col + i];
        if(!_.isNumber(vals[col + i])){
          editableCols++;
        }
      }
      if(cell[desiredWidth] > existingWidth){
        i = 0;
        while(editableCols > 0 && cell[desiredWidth] > existingWidth){
          if(!_.isNumber(vals[col+i])){
            var dif = Math.round( (cell[desiredWidth] - existingWidth) / editableCols );
            existingWidth += dif;
            result[col + i] += dif;
            editableCols--;
          }
          i++;
        }
      }
    }

    _.extend(vals,result);
    for(var j = 0; j < vals.length; j++){
      vals[j] = Math.max(forcedMin, vals[j] || 0);
    }
  };
}
