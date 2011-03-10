// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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
