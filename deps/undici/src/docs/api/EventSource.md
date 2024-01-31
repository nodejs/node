# EventSource

Undici exposes a WHATWG spec-compliant implementation of [EventSource](https://developer.mozilla.org/en-US/docs/Web/API/EventSource)
for [Server-Sent Events](https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events/Using_server-sent_events).

## Instantiating EventSource

Undici exports a EventSource class. You can instantiate the EventSource as
follows:

```mjs
import { EventSource } from 'undici'

const evenSource = new EventSource('http://localhost:3000')
evenSource.onmessage = (event) => {
  console.log(event.data)
}
```

More information about the EventSource API can be found on
[MDN](https://developer.mozilla.org/en-US/docs/Web/API/EventSource).