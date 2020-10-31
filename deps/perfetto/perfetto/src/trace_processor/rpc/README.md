# TraceProcessor RPC

This directory contains the RPC interfaces to use the Perfetto Trace Processor
remotely (i.e. not just in-process). It consists of two targets:

## `wasm_bridge`

The WASM (Web Asssembly) interop bridge. It's used to call the Trace Processor
from HTML/JS using WASM's `ccall`.

## `httpd`

The HTTP RPC module. It exposes a protobuf-over-HTTP RPC interface that allows
interacting with a remote trace processor instance. It's used for special UI
use cases (very large traces > 2GB) and for python interoperability.
