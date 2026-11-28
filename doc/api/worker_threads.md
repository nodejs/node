# Worker threads

<!--introduced_in=v10.5.0-->

<!-- YAML
added: v10.5.0
changes:
  - version: REPLACEME
    pr-url: https://github.com/nodejs/node/pull/00000
    description: Legacy. Use `node:threads/worker` and `node:threads/messaging`
                 instead.
  - version: v12.11.0
    pr-url: https://github.com/nodejs/node/pull/29512
    description: This API is no longer experimental.
  - version: v11.7.0
    pr-url: https://github.com/nodejs/node/pull/25361
    description: This API is no longer behind the `--experimental-worker` CLI flag.
-->

> Stability: 3 - Legacy: Use [`node:threads/worker`][] and
> [`node:threads/messaging`][] instead.

<!-- source_link=lib/worker_threads.js -->

The `node:worker_threads` module is the original path to the APIs for
[threads][]. It is kept unchanged for backward compatibility, but new code
should import them from where they are now:

| `node:worker_threads` export                                                                 | Use instead                   |
| -------------------------------------------------------------------------------------------- | ----------------------------- |
| [`getEnvironmentData()`](threads.md#getenvironmentdatakey)                                   | [`node:threads/worker`][]     |
| [`isInternalThread`](threads.md#isinternalthread)                                            | [`node:threads/worker`][]     |
| [`isMainThread`](threads.md#ismainthread)                                                    | [`node:threads/worker`][]     |
| [`parentPort`](threads.md#parentport)                                                        | [`node:threads/worker`][]     |
| [`postMessageToThread()`](threads.md#postmessagetothreadthreadid-value-transferlist-timeout) | [`node:threads/worker`][]     |
| [`resourceLimits`](threads.md#resourcelimits)                                                | [`node:threads/worker`][]     |
| [`SHARE_ENV`](threads.md#share_env)                                                          | [`node:threads/worker`][]     |
| [`setEnvironmentData()`](threads.md#setenvironmentdatakey-value)                             | [`node:threads/worker`][]     |
| [`threadId`](threads.md#threadid)                                                            | [`node:threads/worker`][]     |
| [`threadName`](threads.md#threadname)                                                        | [`node:threads/worker`][]     |
| [`Worker`](threads.md#class-worker)                                                          | [`node:threads/worker`][]     |
| [`workerData`](threads.md#workerdata)                                                        | [`node:threads/worker`][]     |
| [`isMarkedAsUntransferable()`](threads.md#ismarkedasuntransferableobject)                    | [`node:threads/messaging`][]  |
| [`markAsUncloneable()`](threads.md#markasuncloneableobject)                                  | [`node:threads/messaging`][]  |
| [`markAsUntransferable()`](threads.md#markasuntransferableobject)                            | [`node:threads/messaging`][]  |
| [`moveMessagePortToContext()`](threads.md#movemessageporttocontextport-contextifiedsandbox)  | [`node:threads/messaging`][]  |
| [`receiveMessageOnPort()`](threads.md#receivemessageonportport)                              | [`node:threads/messaging`][]  |
| [`BroadcastChannel`](threads.md#class-broadcastchannel-extends-eventtarget)                  | The `BroadcastChannel` global |
| [`MessageChannel`](threads.md#class-messagechannel)                                          | The `MessageChannel` global   |
| [`MessagePort`](threads.md#class-messageport)                                                | The `MessagePort` global      |
| [`locks`](threads.md#web-locks)                                                              | The `navigator.locks` global  |

The exports are the same objects as at the new locations. For example,
`require('node:worker_threads').Worker` is the same class as
`require('node:threads/worker').Worker`.

[`node:threads/messaging`]: threads.md#the-nodethreadsmessaging-module
[`node:threads/worker`]: threads.md#the-nodethreadsworker-module
[threads]: threads.md
