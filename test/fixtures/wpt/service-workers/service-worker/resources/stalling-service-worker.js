async function post_message_to_client(role, message, ports) {
    (await clients.matchAll()).forEach(client => {
        if (new URL(client.url).searchParams.get('role') === role) {
            client.postMessage(message, ports);
        }
    });
}

async function post_message_to_child(message, ports) {
    await post_message_to_client('child', message, ports);
}

function ping_message(data) {
    return { type: 'ping', data };
}

self.onmessage = event => {
    const message = ping_message(event.data);
    post_message_to_child(message);
    post_message_to_parent(message);
}

async function post_message_to_parent(message, ports) {
    await post_message_to_client('parent', message, ports);
}

function fetch_message(key) {
    return { type: 'fetch', key };
}

// Send a message to the parent along with a MessagePort to respond
// with.
function report_fetch_request(key) {
    const channel = new MessageChannel();
    const reply = new Promise(resolve => {
        channel.port1.onmessage = resolve;
    }).then(event => event.data);
    return post_message_to_parent(fetch_message(key), [channel.port2]).then(() => reply);
}

function respond_with_script(script) {
    return new Response(new Blob(script, { type: 'text/javascript' }));
}

// Whenever a controlled document requests a URL with a 'key' search
// parameter we report the request to the parent frame and wait for
// a response. The content of the response is then used to respond to
// the fetch request.
addEventListener('fetch', event => {
    let key = new URL(event.request.url).searchParams.get('key');
    if (key) {
        event.respondWith(report_fetch_request(key).then(respond_with_script));
    }
});
