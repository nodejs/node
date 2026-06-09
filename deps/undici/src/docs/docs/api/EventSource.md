# EventSource

> ⚠️ Warning: the EventSource API is experimental.

Undici exposes a WHATWG spec-compliant implementation of [EventSource](https://developer.mozilla.org/en-US/docs/Web/API/EventSource)
for [Server-Sent Events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events).

## Instantiating EventSource

Undici exports an EventSource class. You can instantiate the EventSource as
follows:

```mjs
import { EventSource } from 'undici'

const eventSource = new EventSource('http://localhost:3000')
eventSource.onmessage = (event) => {
  console.log(event.data)
}
```

## Receiving events from a server

EventSource connects to an HTTP endpoint that responds with a `text/event-stream`
content type. The connection stays open and receives events as the server writes
them.

```mjs
import { createServer } from 'node:http'
import { EventSource } from 'undici'

const server = createServer((request, response) => {
  response.writeHead(200, {
    'content-type': 'text/event-stream',
    'cache-control': 'no-cache',
    connection: 'keep-alive'
  })

  response.write('event: ping\n')
  response.write('data: connected\n\n')

  const interval = setInterval(() => {
    response.write(`data: ${Date.now()}\n\n`)
  }, 1000)

  request.on('close', () => clearInterval(interval))
})

server.listen(3000, () => {
  const eventSource = new EventSource('http://localhost:3000')

  eventSource.addEventListener('ping', (event) => {
    console.log('ping:', event.data)
  })

  eventSource.onmessage = (event) => {
    console.log('message:', event.data)
  }

  eventSource.onerror = () => {
    eventSource.close()
    server.close()
  }
})
```

The `message` event receives events without an explicit `event:` field. Use
`addEventListener()` to subscribe to named events.

## Using a custom Dispatcher

Undici allows you to set your own Dispatcher in the EventSource constructor.

An example which allows you to modify the request headers is:

```mjs
import { EventSource, Agent } from 'undici'

class CustomHeaderAgent extends Agent {
  dispatch (opts) {
    opts.headers['x-custom-header'] = 'hello world'
    return super.dispatch(...arguments)
  }
}

const eventSource = new EventSource('http://localhost:3000', {
  dispatcher: new CustomHeaderAgent()
})
```

More information about the EventSource API can be found on
[MDN](https://developer.mozilla.org/en-US/docs/Web/API/EventSource).
