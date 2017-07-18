interface Ignore {
  /**
   * Adds a rule rules to the current manager.
   * @param  {string | Ignore} pattern
   * @returns IgnoreBase
   */
  add(pattern: string | Ignore): Ignore
  /**
   * Adds several rules to the current manager.
   * @param  {string[]} patterns
   * @returns IgnoreBase
   */
  add(patterns: (string | Ignore)[]): Ignore

  /**
   * Filters the given array of pathnames, and returns the filtered array.
   * NOTICE that each path here should be a relative path to the root of your repository.
   * @param  {string[]} paths the array of paths to be filtered.
   * @returns IgnoreBase
   */
  filter(paths: string[]): Ignore
  /**
   * Creates a filter function which could filter 
   * an array of paths with Array.prototype.filter.
   */
  createFilter(): (path: string) => Ignore

  /**
   * Returns Boolean whether pathname should be ignored.
   * @param  {string} pathname a path to check
   * @returns boolean
   */
  ignores(pathname: string): boolean
}

/**
 * Creates new ignore manager.
 */
declare function ignore(): Ignore

export = ignore 
