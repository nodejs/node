export namespace util {
  /**
   * Retrieves a header name and returns its lowercase value.
   * @param value Header name
   */
  export function headerNameToString(value: string | Buffer): string;

  /**
   * Receives a header object and returns the parsed value.
   * @param headers Header object
   */
  export function parseHeaders(
    headers:
      | Record<string, string | string[]>
      | (Buffer | string | (Buffer | string)[])[]
  ): Record<string, string | string[]>;
  /**
   * Receives a header object and returns the parsed value.
   * @param headers Header object
   * @param obj Object to specify a proxy object. Used to assign parsed values. But, if `headers` is an object, it is not used.
   * @returns If `headers` is an object, it is `headers`. Otherwise, if `obj` is specified, it is equivalent to `obj`.
   */
  export function parseHeaders<
    H extends
      | Record<string, string | string[]>
      | (Buffer | string | (Buffer | string)[])[]
  >(
    headers: H,
    obj?: H extends any[] ? Record<string, string | string[]> : never
  ): Record<string, string | string[]>;
}
