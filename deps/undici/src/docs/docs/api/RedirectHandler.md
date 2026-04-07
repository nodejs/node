# Class: RedirectHandler

A class that handles redirection logic for HTTP requests.

## `new RedirectHandler(dispatch, maxRedirections, opts, handler)`

Arguments:

- **dispatch** `function` - The dispatch function to be called after every retry.
- **maxRedirections** `number` - Maximum number of redirections allowed.
- **opts** `object` - Options for handling redirection.
- **handler** `object` - An object containing handlers for different stages of the request lifecycle.

Returns: `RedirectHandler`

### Parameters

- **dispatch** `(options: Dispatch.DispatchOptions, handlers: Dispatch.DispatchHandler) => Promise<Dispatch.DispatchResponse>` (required) - Dispatch function to be called after every redirection.
- **maxRedirections** `number` (required) - Maximum number of redirections allowed.
- **opts** `object` (required) - Options for handling redirection.
- **handler** `object` (required) - Handlers for different stages of the request lifecycle.

### Properties

- **location** `string` - The current redirection location.
- **abort** `function` - The abort function.
- **opts** `object` - The options for handling redirection.
- **maxRedirections** `number` - Maximum number of redirections allowed.
- **handler** `object` - Handlers for different stages of the request lifecycle.
- **history** `Array` - An array representing the history of URLs during redirection.

### Methods

#### `onRequestStart(controller, context)`

Called when the request starts.

Parameters:

- **controller** `DispatchController` - The request controller.
- **context** `object` - The dispatch context.

#### `onRequestUpgrade(controller, statusCode, headers, socket)`

Called when an upgrade is requested.

Parameters:

- **controller** `DispatchController` - The request controller.
- **statusCode** `number` - The HTTP status code.
- **headers** `object` - The headers received in the response.
- **socket** `object` - The socket object.

#### `onResponseError(controller, error)`

Called when an error occurs.

Parameters:

- **controller** `DispatchController` - The request controller.
- **error** `Error` - The error that occurred.

#### `onResponseStart(controller, statusCode, headers, statusText)`

Called when headers are received.

Parameters:

- **controller** `DispatchController` - The request controller.
- **statusCode** `number` - The HTTP status code.
- **headers** `object` - The headers received in the response.
- **statusText** `string` - The status text.

#### `onResponseData(controller, chunk)`

Called when data is received.

Parameters:

- **controller** `DispatchController` - The request controller.
- **chunk** `Buffer` - The data chunk received.

#### `onResponseEnd(controller, trailers)`

Called when the request is complete.

Parameters:

- **controller** `DispatchController` - The request controller.
- **trailers** `object` - The trailers received.

#### `onBodySent(chunk)`

Called when the request body is sent.

Parameters:

- **chunk** `Buffer` - The chunk of the request body sent.
