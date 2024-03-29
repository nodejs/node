# Fetch

Undici exposes a fetch() method starts the process of fetching a resource from the network.

Documentation and examples can be found on [MDN](https://developer.mozilla.org/en-US/docs/Web/API/fetch).

## File

This API is implemented as per the standard, you can find documentation on [MDN](https://developer.mozilla.org/en-US/docs/Web/API/File)

In Node versions v18.13.0 and above and v19.2.0 and above, undici will default to using Node's [File](https://nodejs.org/api/buffer.html#class-file) class. In versions where it's not available, it will default to the undici one.

## FormData

This API is implemented as per the standard, you can find documentation on [MDN](https://developer.mozilla.org/en-US/docs/Web/API/FormData).

If any parameters are passed to the FormData constructor other than `undefined`, an error will be thrown. Other parameters are ignored.

## Response

This API is implemented as per the standard, you can find documentation on [MDN](https://developer.mozilla.org/en-US/docs/Web/API/Response)

## Request

This API is implemented as per the standard, you can find documentation on [MDN](https://developer.mozilla.org/en-US/docs/Web/API/Request)

## Header

This API is implemented as per the standard, you can find documentation on [MDN](https://developer.mozilla.org/en-US/docs/Web/API/Headers)

# Body Mixins

`Response` and `Request` body inherit body mixin methods. These methods include:

- [`.arrayBuffer()`](https://fetch.spec.whatwg.org/#dom-body-arraybuffer)
- [`.blob()`](https://fetch.spec.whatwg.org/#dom-body-blob)
- [`.formData()`](https://fetch.spec.whatwg.org/#dom-body-formdata)
- [`.json()`](https://fetch.spec.whatwg.org/#dom-body-json)
- [`.text()`](https://fetch.spec.whatwg.org/#dom-body-text)

There is an ongoing discussion regarding `.formData()` and its usefulness and performance in server environments. It is recommended to use a dedicated library for parsing `multipart/form-data` bodies, such as [Busboy](https://www.npmjs.com/package/busboy) or [@fastify/busboy](https://www.npmjs.com/package/@fastify/busboy).

These libraries can be interfaced with fetch with the following example code:

```mjs
import { Busboy } from '@fastify/busboy'
import { Readable } from 'node:stream'

const response = await fetch('...')
const busboy = new Busboy({
  headers: {
    'content-type': response.headers.get('content-type')
  }
})

Readable.fromWeb(response.body).pipe(busboy)
```
