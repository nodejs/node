importScripts('/common/get-host-info.sub.js');
importScripts('test-helpers.sub.js');
importScripts('/resources/testharness.js');

const storedResponse = new Response(new Blob(['a simple text file']))
const absolultePath = `${base_path()}/simple.txt`

self.addEventListener('fetch', event => {
    const search = new URLSearchParams(new URL(event.request.url).search.substr(1))
    const variant = search.get('variant')
    const delay = search.get('delay')
    if (!variant)
        return

    switch (variant) {
        case 'forward':
            event.respondWith(fetch(event.request.url))
            break
        case 'redirect':
            event.respondWith(fetch(`/xhr/resources/redirect.py?location=${base_path()}/simple.txt`))
            break
        case 'delay-before-fetch':
            event.respondWith(
                new Promise(resolve => {
                    step_timeout(() => fetch(event.request.url).then(resolve), delay)
            }))
            break
        case 'delay-after-fetch':
            event.respondWith(new Promise(resolve => {
                fetch(event.request.url)
                    .then(response => step_timeout(() => resolve(response), delay))
            }))
            break
    }
});
