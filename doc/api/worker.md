# Worker

<!--introduced_in=REPLACEME-->

> Stability: 1 - Experimental

## Class: MessageChannel
<!-- YAML
added: REPLACEME
-->

Instances of the `worker.MessageChannel` class represent an asynchronous,
two-way communications channel.
The `MessageChannel` has no methods of its own. `new MessageChannel()`
yields an object with `port1` and `port2` properties, which refer to linked
[`MessagePort`][] instances.

```js
const { MessageChannel } = require('worker');

const { port1, port2 } = new MessageChannel();
port1.on('message', (message) => console.log('received', message));
port2.postMessage({ foo: 'bar' });
// prints: received { foo: 'bar' }
```

## Class: MessagePort
<!-- YAML
added: REPLACEME
-->

* Extends: {EventEmitter}

Instances of the `worker.MessagePort` class represent one end of an
asynchronous, two-way communications channel. It can be used to transfer
structured data, memory regions and other `MessagePort`s between different
[`Worker`][]s.

With the exception of `MessagePort`s being [`EventEmitter`][]s rather
than `EventTarget`s, this implementation matches [browser `MessagePort`][]s.

### Event: 'close'
<!-- YAML
added: REPLACEME
-->

The `'close'` event is emitted once either side of the channel has been
disconnected.

### Event: 'message'
<!-- YAML
added: REPLACEME
-->

* `value` {any} The transmitted value

The `'message'` event is emitted for any incoming message, containing the cloned
input of [`port.postMessage()`][].

Listeners on this event will receive a clone of the `value` parameter as passed
to `postMessage()` and no further arguments.

### port.close()
<!-- YAML
added: REPLACEME
-->

Disables further sending of messages on either side of the connection.
This method can be called once you know that no further communication
will happen over this `MessagePort`.

### port.postMessage(value[, transferList])
<!-- YAML
added: REPLACEME
-->

* `value` {any}
* `transferList` {Object[]}

Sends a JavaScript value to the receiving side of this channel.
`value` will be transferred in a way which is compatible with
the [HTML structured clone algorithm][]. In particular, it may contain circular
references and objects like typed arrays that the `JSON` API is not able
to stringify.

`transferList` may be a list of `ArrayBuffer` objects.
After transferring, they will not be usable on the sending side of the channel
anymore (even if they are not contained in `value`).

`value` may still contain `ArrayBuffer` instances that are not in
`transferList`; in that case, the underlying memory is copied rather than moved.

For more information on the serialization and deserialization mechanisms
behind this API, see the [serialization API of the `v8` module][v8.serdes].

Because the object cloning uses the structured clone algorithm,
non-enumerable properties, property accessors, and object prototypes are
not preserved. In particular, [`Buffer`][] objects will be read as
plain [`Uint8Array`][]s on the receiving side.

The message object will be cloned immediately, and can be modified after
posting without having side effects.

### port.ref()
<!-- YAML
added: REPLACEME
-->

Opposite of `unref()`. Calling `ref()` on a previously `unref()`ed port will
*not* let the program exit if it's the only active handle left (the default
behavior). If the port is `ref()`ed, calling `ref()` again will have no effect.

If listeners are attached or removed using `.on('message')`, the port will
be `ref()`ed and `unref()`ed automatically depending on whether
listeners for the event exist.

### port.start()
<!-- YAML
added: REPLACEME
-->

Starts receiving messages on this `MessagePort`. When using this port
as an event emitter, this will be called automatically once `'message'`
listeners are attached.

### port.unref()
<!-- YAML
added: REPLACEME
-->

Calling `unref()` on a port will allow the thread to exit if this is the only
active handle in the event system. If the port is already `unref()`ed calling
`unref()` again will have no effect.

If listeners are attached or removed using `.on('message')`, the port will
be `ref()`ed and `unref()`ed automatically depending on whether
listeners for the event exist.

[`Buffer`]: buffer.html
[`EventEmitter`]: events.html
[`MessagePort`]: #worker_class_messageport
[`port.postMessage()`]: #worker_port_postmessage_value_transferlist
[v8.serdes]: v8.html#v8_serialization_api
[`Uint8Array`]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array
[browser `MessagePort`]: https://developer.mozilla.org/en-US/docs/Web/API/MessagePort
[HTML structured clone algorithm]: https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Structured_clone_algorithm
