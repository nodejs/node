const staticAssets = [
  'assets/style.css',
  'assets/sh.css',
  'assets/sh_main.js',
  'assets/sh_javascript.min.js',
  'assets/offline_docs.js',
  'all.html'
]

function findVersionFromUrl() {
  const matches = window.location.pathname.match(/v\d+\.\d+\.\d+/)
  return matches && matches[0]
}

function isLocalDocURL(url) {
  return !url.startsWith('http')
}

function isLocalDevelopment() {
  return window.location.host.includes('localhost')
}

function findUrlsToAllDocLinks() {
  const linkElements = Array.from(document.querySelectorAll('#column2 a'))
  const urls = linkElements.map(elem => elem.attributes.href.value)

  return urls.filter(isLocalDocURL)
}

function removePreviousFeedbacks(parent) {
  parent.querySelectorAll('[data-js-feedback]').forEach(element => element.remove())
}

function cacheDocs(event) {
  event.preventDefault()

  const nodeVersion = findVersionFromUrl()
  const docUrls = findUrlsToAllDocLinks()
  const urls = staticAssets.concat(docUrls)
  const display = (text) => () => displayTextBelow(event.target, text)

  if (nodeVersion || isLocalDevelopment()) {
    removePreviousFeedbacks(event.target.parentNode)

    caches.open('v1')
      .then(cache => cache.addAll(urls))
      .then(display(`✓ docs for ${nodeVersion} are now available while offline.`),
            display('An error occured while downloading docs, probably already offline?'))
  }
}

function cacheDocsElemOnClick(fn) {
  const elements = Array.from(document.querySelectorAll('[data-js-handler="offline-docs"]'))

  elements.forEach(elem => elem.addEventListener('click', fn))
}

function displayNoSwSupport(event) {
  displayTextBelow(event.target, 'Sorry, your browser do not have support for making docs offline yet :(')
}

function displayTextBelow(element, text) {
  const feedbackElem = document.createElement('p')
  feedbackElem.textContent = text
  feedbackElem.setAttribute('data-js-feedback', '1')
  feedbackElem.style.color = 'white'
  feedbackElem.style.fontStyle = 'italic'
  feedbackElem.style.marginTop = '10px'

  element.parentNode.appendChild(feedbackElem)
}

if ('serviceWorker' in navigator) {
  navigator.serviceWorker.register('service_worker.js')
  cacheDocsElemOnClick(cacheDocs)
} else {
  cacheDocsElemOnClick(displayNoSwSupport)
}
