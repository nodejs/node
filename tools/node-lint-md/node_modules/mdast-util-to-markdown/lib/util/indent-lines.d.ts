/**
 * @param {string} value
 * @param {Map} map
 * @returns {string}
 */
export function indentLines(value: string, map: Map): string
export type Map = (value: string, line: number, blank: boolean) => string
