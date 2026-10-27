declare module "@tc39/source-map-tests/suites*.json" with { type: "json" } {
  const index: import("./types.js").SuitesIndex;
  export default index;
}

declare module "@tc39/source-map-tests/interpreting/*.json" with { type: "json" } {
  const suite: import("./types.js").InterpretingSuite;
  export default suite;
}

declare module "@tc39/source-map-tests/decoding/*.json" with { type: "json" } {
  const suite: import("./types.js").DecodingSuite;
  export default suite;
}
