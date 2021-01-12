// META: global=window,worker
// META: script=/resources/WebIDLParser.js
// META: script=/resources/idlharness.js
// META: timeout=long

idl_test(
  ['fetch'],
  ['referrer-policy', 'html', 'dom'],
  idl_array => {
    idl_array.add_objects({
      Headers: ["new Headers()"],
      Request: ["new Request('about:blank')"],
      Response: ["new Response()"],
    });
    if (self.GLOBAL.isWindow()) {
      idl_array.add_objects({ Window: ['window'] });
    } else if (self.GLOBAL.isWorker()) {
      idl_array.add_objects({ WorkerGlobalScope: ['self'] });
    }
  }
);
