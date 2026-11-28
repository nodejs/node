export class URLPattern {
  protocol: string
  username: string
  password: string
  hostname: string
  port: string
  pathname: string
  search: string
  hash: string

  constructor(input: Record<string, string> | string, options?: { ignoreCase: boolean });
  constructor(input: Record<string, string> | string, baseUrl?: string, options?: { ignoreCase: boolean });

  exec(input: string | Record<string, string>, baseURL?: string): null | Record<string, unknown>;
  test(input: string | Record<string, string>, baseURL?: string): boolean;
}

export class URLPatternList {
  constructor(patterns: Iterable<URLPattern>);

  exec(input?: string | Record<string, string> | null, baseURL?: string | null): null | Record<string, unknown>;
  test(input?: string | Record<string, string> | null, baseURL?: string | null): boolean;
}

export interface URLPatternBinding {
  URLPattern: URLPattern;
  URLPatternList: URLPatternList;
}
