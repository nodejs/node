/**
 * @typedef URL
 * @property {string} hash
 * @property {string} host
 * @property {string} hostname
 * @property {string} href
 * @property {string} origin
 * @property {string} password
 * @property {string} pathname
 * @property {string} port
 * @property {string} protocol
 * @property {string} search
 * @property {any} searchParams
 * @property {string} username
 * @property {() => string} toString
 * @property {() => string} toJSON
 */
/**
 * @param {unknown} fileURLOrPath
 * @returns {fileURLOrPath is URL}
 */
export function isUrl(fileURLOrPath: unknown): fileURLOrPath is URL
export type URL = {
  hash: string
  host: string
  hostname: string
  href: string
  origin: string
  password: string
  pathname: string
  port: string
  protocol: string
  search: string
  searchParams: any
  username: string
  toString: () => string
  toJSON: () => string
}
