'use strict'
const esNextText = 'Modern JavaScript:'
const es5Text = 'ES5:'

function nextSiblingMatch (e, name, text) {
  var sib = e.nextSibling
  while (sib) {
    if (sib.nodeType == 1) {
      if (sib.nodeName == name && (!text || sib.textContent.indexOf(esNextText) > -1))
        return sib
      return null
    }
    sib = sib.nextSibling
  }
}

function renderESCodeBlocks() {
  Array.prototype.slice.call(document.querySelectorAll('p'))
    .filter(function (p) {
      return p.textContent.indexOf(esNextText) > -1})
    .map(function (esNextP) {
      var esNextPre = nextSiblingMatch(esNextP, 'PRE')
      if (!esNextPre) return null
      var es5P = nextSiblingMatch(esNextPre, 'P')
      if (!es5P) return null
      var es5Pre = nextSiblingMatch(es5P, 'PRE')
      if (!es5Pre) return null
      return { esNextP: esNextP, esNextPre: esNextPre, es5P: es5P, es5Pre: es5Pre }
    })
    .filter(Boolean)
    .forEach(function (block) {
      var div = document.createElement('div')
      div.className = 'es-switcher'
      block.esNextP.parentElement.insertBefore(div, block.esNextP)
      block.esNextP.textContent = esNextText.replace(/:$/, '')
      block.es5P.textContent = es5Text.replace(/:$/, '')
      block.esNextP.className = 'active'
      div.appendChild(block.esNextP)
      div.appendChild(block.es5P)
      div.appendChild(block.esNextPre)
      div.appendChild(block.es5Pre)
      block.es5Pre.style.display = 'none'
      block.esNextP.addEventListener('click', function () {
        block.esNextPre.style.display = 'block'
        block.es5Pre.style.display = 'none'
        block.esNextP.className = 'active'
        block.es5P.className = ''
      })
      block.es5P.addEventListener('click', function () {
        block.esNextPre.style.display = 'none'
        block.es5Pre.style.display = 'block'
        block.esNextP.className = ''
        block.es5P.className = 'active'
      })
      div.insertAdjacentHTML('afterend', '<br>')
    })
}

renderESCodeBlocks()
