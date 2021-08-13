// Export `JSX` as a global for TypeScript.
import {runtime} from './runtime.js'
import {s} from './svg.js'

export * from './jsx-automatic.js'
export const {Fragment, jsx, jsxs} = runtime(s)
