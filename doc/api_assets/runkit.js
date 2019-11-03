'use strict';

(function() {
  // https://developer.mozilla.org/en-US/docs/Web/API/ChildNode/replaceWith
  function ReplaceWithPolyfill() {
    'use-strict'; // For safari, and IE > 10
    var parent = this.parentNode, i = arguments.length, currentNode;
    if (!parent) return;
    if (!i) // if there are no arguments
      parent.removeChild(this);
    while (i--) { // i-- decrements i and returns the value of i before the decrement
      currentNode = arguments[i];
      if (typeof currentNode !== 'object'){
        currentNode = this.ownerDocument.createTextNode(currentNode);
      } else if (currentNode.parentNode){
        currentNode.parentNode.removeChild(currentNode);
      }
      // the value of "i" below is after the decrement
      if (!i) // if currentNode is the first argument (currentNode === arguments[0])
        parent.replaceChild(currentNode, this);
      else // if currentNode isn't the first
        parent.insertBefore(currentNode, this.previousSibling);
    }
  }
  if (!Element.prototype.replaceWith)
      Element.prototype.replaceWith = ReplaceWithPolyfill;
  if (!CharacterData.prototype.replaceWith)
      CharacterData.prototype.replaceWith = ReplaceWithPolyfill;
  if (!DocumentType.prototype.replaceWith) 
      DocumentType.prototype.replaceWith = ReplaceWithPolyfill;
}());

(function() {
  var runnables = document.querySelectorAll('.runkit');
  runnables.forEach(function(runnable) {
    var source = RunKit.sourceFromElement(runnable);
    var wrapper = document.createElement('div');
    wrapper.class = 'runkit-wrapper';
    wrapper.style = 'top: 0; left: -9999px; margin: 1rem;';
    RunKit.createNotebook({
      element: wrapper,
      minHeight: '0px',
      source: source,
    });
    runnable.parentElement.replaceWith(wrapper);
  });
}());
