// NOTE: These definitions support NodeJS and TypeScript 3.6 and earlier.

// NOTE: TypeScript version-specific augmentations can be found in the following paths:
//          - ~/base.d.ts         - Shared definitions common to all TypeScript versions
//          - ~/index.d.ts        - Definitions specific to TypeScript 3.7 and above
//          - ~/ts3.6/base.d.ts   - Definitions specific to TypeScript 3.6 and earlier
//          - ~/ts3.6/index.d.ts  - Definitions specific to TypeScript 3.6 and earlier with assert pulled in

// Reference required types from the default lib:
/// <reference lib="es2020" />
/// <reference lib="esnext.asynciterable" />
/// <reference lib="esnext.intl" />
/// <reference lib="esnext.bigint" />

// Base definitions for all NodeJS modules that are not specific to any version of TypeScript:
/// <reference path="../assert/strict.d.ts" />
/// <reference path="../globals.d.ts" />
/// <reference path="../async_hooks.d.ts" />
/// <reference path="../buffer.d.ts" />
/// <reference path="../child_process.d.ts" />
/// <reference path="../cluster.d.ts" />
/// <reference path="../console.d.ts" />
/// <reference path="../constants.d.ts" />
/// <reference path="../crypto.d.ts" />
/// <reference path="../dgram.d.ts" />
/// <reference path="../diagnostics_channel.d.ts" />
/// <reference path="../dns.d.ts" />
/// <reference path="../dns/promises.d.ts" />
/// <reference path="../dns/promises.d.ts" />
/// <reference path="../domain.d.ts" />
/// <reference path="../events.d.ts" />
/// <reference path="../fs.d.ts" />
/// <reference path="../fs/promises.d.ts" />
/// <reference path="../http.d.ts" />
/// <reference path="../http2.d.ts" />
/// <reference path="../https.d.ts" />
/// <reference path="../inspector.d.ts" />
/// <reference path="../module.d.ts" />
/// <reference path="../net.d.ts" />
/// <reference path="../os.d.ts" />
/// <reference path="../path.d.ts" />
/// <reference path="../perf_hooks.d.ts" />
/// <reference path="../process.d.ts" />
/// <reference path="../punycode.d.ts" />
/// <reference path="../querystring.d.ts" />
/// <reference path="../readline.d.ts" />
/// <reference path="../repl.d.ts" />
/// <reference path="../stream.d.ts" />
/// <reference path="../stream/promises.d.ts" />
/// <reference path="../stream/web.d.ts" />
/// <reference path="../string_decoder.d.ts" />
/// <reference path="../timers.d.ts" />
/// <reference path="../timers/promises.d.ts" />
/// <reference path="../tls.d.ts" />
/// <reference path="../trace_events.d.ts" />
/// <reference path="../tty.d.ts" />
/// <reference path="../url.d.ts" />
/// <reference path="../util.d.ts" />
/// <reference path="../v8.d.ts" />
/// <reference path="../vm.d.ts" />
/// <reference path="../worker_threads.d.ts" />
/// <reference path="../zlib.d.ts" />

// TypeScript 3.6-specific augmentations:
/// <reference path="../globals.global.d.ts" />

// TypeScript 3.6-specific augmentations:
/// <reference path="../wasi.d.ts" />
