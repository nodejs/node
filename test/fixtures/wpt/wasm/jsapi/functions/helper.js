function call_later(f) {
  const builder = new WasmModuleBuilder();
  const functionIndex = builder.addImport("module", "imported", kSig_v_v);
  builder.addStart(functionIndex);
  const buffer = builder.toBuffer();

  WebAssembly.instantiate(buffer, {
    "module": {
      "imported": f,
    }
  });
}
