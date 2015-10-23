'use strict';

function init(list) {
  list._idleNext = list;
  list._idlePrev = list;
}
exports.init = init;


// show the most idle item
function peek(list) {
  if (list._idlePrev == list) return null;
  return list._idlePrev;
}
exports.peek = peek;


// remove the most idle item from the list
function shift(list) {
  var first = list._idlePrev;
  remove(first);
  return first;
}
exports.shift = shift;


// remove a item from its list
function remove(item) {
  if (item._idleNext) {
    item._idleNext._idlePrev = item._idlePrev;
  }

  if (item._idlePrev) {
    item._idlePrev._idleNext = item._idleNext;
  }

  item._idleNext = null;
  item._idlePrev = null;
}
exports.remove = remove;


// remove a item from its list and place at the end.
function append(list, item) {
  remove(item);
  item._idleNext = list._idleNext;
  list._idleNext._idlePrev = item;
  item._idlePrev = list;
  list._idleNext = item;
}
exports.append = append;


function isEmpty(list) {
  return list._idleNext === list;
}
exports.isEmpty = isEmpty;
