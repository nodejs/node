'use strict';

function init(list) {
  list._idleNext = list;
  list._idlePrev = list;
  return list;
}

// Show the most idle item.
function peek(list) {
  if (list._idlePrev === list) return null;
  return list._idlePrev;
}

// Remove an item from its list.
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

// Remove an item from its list and place at the end.
function append(list, item) {
  if (item._idleNext || item._idlePrev) {
    remove(item);
  }

  // Items are linked  with _idleNext -> (older) and _idlePrev -> (newer).
  // Note: This linkage (next being older) may seem counter-intuitive at first.
  item._idleNext = list._idleNext;
  item._idlePrev = list;

  // The list _idleNext points to tail (newest) and _idlePrev to head (oldest).
  list._idleNext._idlePrev = item;
  list._idleNext = item;
}

function isEmpty(list) {
  return list._idleNext === list;
}

module.exports = {
  init,
  peek,
  remove,
  append,
  isEmpty,
};
