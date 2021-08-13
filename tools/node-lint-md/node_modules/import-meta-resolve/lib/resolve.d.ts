/// <reference types="node" />
/**
 * @param {string} url
 * @returns {PackageType}
 */
export function getPackageType(url: string): PackageType
/**
 * The “Resolver Algorithm Specification” as detailed in the Node docs (which is
 * sync and slightly lower-level than `resolve`).
 *
 *
 *
 * @param {string} specifier
 * @param {URL} base
 * @param {Set<string>} [conditions]
 * @returns {URL}
 */
export function moduleResolve(
  specifier: string,
  base: URL,
  conditions?: Set<string>
): URL
/**
 * @param {string} specifier
 * @param {{parentURL?: string, conditions?: string[]}} context
 * @returns {{url: string}}
 */
export function defaultResolve(
  specifier: string,
  context?: {
    parentURL?: string
    conditions?: string[]
  }
): {
  url: string
}
export type PackageType = 'module' | 'commonjs' | 'none'
export type PackageConfig = {
  pjsonPath: string
  exists: boolean
  main: string
  name: string
  type: PackageType
  exports: {
    [x: string]: unknown
  }
  imports: {
    [x: string]: unknown
  }
}
export type ResolveObject = {
  resolved: URL
  exact: boolean
}
import {URL} from 'url'
