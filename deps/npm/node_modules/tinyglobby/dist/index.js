"use strict";
var __create = Object.create;
var __defProp = Object.defineProperty;
var __getOwnPropDesc = Object.getOwnPropertyDescriptor;
var __getOwnPropNames = Object.getOwnPropertyNames;
var __getProtoOf = Object.getPrototypeOf;
var __hasOwnProp = Object.prototype.hasOwnProperty;
var __export = (target, all) => {
  for (var name in all)
    __defProp(target, name, { get: all[name], enumerable: true });
};
var __copyProps = (to, from, except, desc) => {
  if (from && typeof from === "object" || typeof from === "function") {
    for (let key of __getOwnPropNames(from))
      if (!__hasOwnProp.call(to, key) && key !== except)
        __defProp(to, key, { get: () => from[key], enumerable: !(desc = __getOwnPropDesc(from, key)) || desc.enumerable });
  }
  return to;
};
var __toESM = (mod, isNodeMode, target) => (target = mod != null ? __create(__getProtoOf(mod)) : {}, __copyProps(
  // If the importer is in node compatibility mode or this is not an ESM
  // file that has been converted to a CommonJS file using a Babel-
  // compatible transform (i.e. "__esModule" has not been set), then set
  // "default" to the CommonJS "module.exports" for node compatibility.
  isNodeMode || !mod || !mod.__esModule ? __defProp(target, "default", { value: mod, enumerable: true }) : target,
  mod
));
var __toCommonJS = (mod) => __copyProps(__defProp({}, "__esModule", { value: true }), mod);

// src/index.ts
var index_exports = {};
__export(index_exports, {
  convertPathToPattern: () => convertPathToPattern,
  escapePath: () => escapePath,
  glob: () => glob,
  globSync: () => globSync,
  isDynamicPattern: () => isDynamicPattern
});
module.exports = __toCommonJS(index_exports);
var import_node_path = __toESM(require("path"));
var import_fdir = require("fdir");
var import_picomatch2 = __toESM(require("picomatch"));

// src/utils.ts
var import_picomatch = __toESM(require("picomatch"));
var ONLY_PARENT_DIRECTORIES = /^(\/?\.\.)+$/;
function getPartialMatcher(patterns, options) {
  const patternsCount = patterns.length;
  const patternsParts = Array(patternsCount);
  const regexes = Array(patternsCount);
  for (let i = 0; i < patternsCount; i++) {
    const parts = splitPattern(patterns[i]);
    patternsParts[i] = parts;
    const partsCount = parts.length;
    const partRegexes = Array(partsCount);
    for (let j = 0; j < partsCount; j++) {
      partRegexes[j] = import_picomatch.default.makeRe(parts[j], options);
    }
    regexes[i] = partRegexes;
  }
  return (input) => {
    const inputParts = input.split("/");
    if (inputParts[0] === ".." && ONLY_PARENT_DIRECTORIES.test(input)) {
      return true;
    }
    for (let i = 0; i < patterns.length; i++) {
      const patternParts = patternsParts[i];
      const regex = regexes[i];
      const inputPatternCount = inputParts.length;
      const minParts = Math.min(inputPatternCount, patternParts.length);
      let j = 0;
      while (j < minParts) {
        const part = patternParts[j];
        if (part.includes("/")) {
          return true;
        }
        const match = regex[j].test(inputParts[j]);
        if (!match) {
          break;
        }
        if (part === "**") {
          return true;
        }
        j++;
      }
      if (j === inputPatternCount) {
        return true;
      }
    }
    return false;
  };
}
var splitPatternOptions = { parts: true };
function splitPattern(path2) {
  var _a;
  const result = import_picomatch.default.scan(path2, splitPatternOptions);
  return ((_a = result.parts) == null ? void 0 : _a.length) ? result.parts : [path2];
}
var isWin = process.platform === "win32";
var ESCAPED_WIN32_BACKSLASHES = /\\(?![()[\]{}!+@])/g;
function convertPosixPathToPattern(path2) {
  return escapePosixPath(path2);
}
function convertWin32PathToPattern(path2) {
  return escapeWin32Path(path2).replace(ESCAPED_WIN32_BACKSLASHES, "/");
}
var convertPathToPattern = isWin ? convertWin32PathToPattern : convertPosixPathToPattern;
var POSIX_UNESCAPED_GLOB_SYMBOLS = /(?<!\\)([()[\]{}*?|]|^!|[!+@](?=\()|\\(?![()[\]{}!*+?@|]))/g;
var WIN32_UNESCAPED_GLOB_SYMBOLS = /(?<!\\)([()[\]{}]|^!|[!+@](?=\())/g;
var escapePosixPath = (path2) => path2.replace(POSIX_UNESCAPED_GLOB_SYMBOLS, "\\$&");
var escapeWin32Path = (path2) => path2.replace(WIN32_UNESCAPED_GLOB_SYMBOLS, "\\$&");
var escapePath = isWin ? escapeWin32Path : escapePosixPath;
function isDynamicPattern(pattern, options) {
  if ((options == null ? void 0 : options.caseSensitiveMatch) === false) {
    return true;
  }
  const scan = import_picomatch.default.scan(pattern);
  return scan.isGlob || scan.negated;
}
function log(...tasks) {
  console.log(`[tinyglobby ${(/* @__PURE__ */ new Date()).toLocaleTimeString("es")}]`, ...tasks);
}

// src/index.ts
var PARENT_DIRECTORY = /^(\/?\.\.)+/;
var ESCAPING_BACKSLASHES = /\\(?=[()[\]{}!*+?@|])/g;
var BACKSLASHES = /\\/g;
function normalizePattern(pattern, expandDirectories, cwd, props, isIgnore) {
  var _a;
  let result = pattern;
  if (pattern.endsWith("/")) {
    result = pattern.slice(0, -1);
  }
  if (!result.endsWith("*") && expandDirectories) {
    result += "/**";
  }
  if (import_node_path.default.isAbsolute(result.replace(ESCAPING_BACKSLASHES, ""))) {
    result = import_node_path.posix.relative(escapePath(cwd), result);
  } else {
    result = import_node_path.posix.normalize(result);
  }
  const parentDirectoryMatch = PARENT_DIRECTORY.exec(result);
  if (parentDirectoryMatch == null ? void 0 : parentDirectoryMatch[0]) {
    const potentialRoot = import_node_path.posix.join(cwd, parentDirectoryMatch[0]);
    if (props.root.length > potentialRoot.length) {
      props.root = potentialRoot;
      props.depthOffset = -(parentDirectoryMatch[0].length + 1) / 3;
    }
  } else if (!isIgnore && props.depthOffset >= 0) {
    const parts = splitPattern(result);
    (_a = props.commonPath) != null ? _a : props.commonPath = parts;
    const newCommonPath = [];
    const length = Math.min(props.commonPath.length, parts.length);
    for (let i = 0; i < length; i++) {
      const part = parts[i];
      if (part === "**" && !parts[i + 1]) {
        newCommonPath.pop();
        break;
      }
      if (part !== props.commonPath[i] || isDynamicPattern(part) || i === parts.length - 1) {
        break;
      }
      newCommonPath.push(part);
    }
    props.depthOffset = newCommonPath.length;
    props.commonPath = newCommonPath;
    props.root = newCommonPath.length > 0 ? import_node_path.default.posix.join(cwd, ...newCommonPath) : cwd;
  }
  return result;
}
function processPatterns({ patterns, ignore = [], expandDirectories = true }, cwd, props) {
  if (typeof patterns === "string") {
    patterns = [patterns];
  } else if (!patterns) {
    patterns = ["**/*"];
  }
  if (typeof ignore === "string") {
    ignore = [ignore];
  }
  const matchPatterns = [];
  const ignorePatterns = [];
  for (const pattern of ignore) {
    if (!pattern) {
      continue;
    }
    if (pattern[0] !== "!" || pattern[1] === "(") {
      ignorePatterns.push(normalizePattern(pattern, expandDirectories, cwd, props, true));
    }
  }
  for (const pattern of patterns) {
    if (!pattern) {
      continue;
    }
    if (pattern[0] !== "!" || pattern[1] === "(") {
      matchPatterns.push(normalizePattern(pattern, expandDirectories, cwd, props, false));
    } else if (pattern[1] !== "!" || pattern[2] === "(") {
      ignorePatterns.push(normalizePattern(pattern.slice(1), expandDirectories, cwd, props, true));
    }
  }
  return { match: matchPatterns, ignore: ignorePatterns };
}
function getRelativePath(path2, cwd, root) {
  return import_node_path.posix.relative(cwd, `${root}/${path2}`) || ".";
}
function processPath(path2, cwd, root, isDirectory, absolute) {
  const relativePath = absolute ? path2.slice(root === "/" ? 1 : root.length + 1) || "." : path2;
  if (root === cwd) {
    return isDirectory && relativePath !== "." ? relativePath.slice(0, -1) : relativePath;
  }
  return getRelativePath(relativePath, cwd, root);
}
function formatPaths(paths, cwd, root) {
  for (let i = paths.length - 1; i >= 0; i--) {
    const path2 = paths[i];
    paths[i] = getRelativePath(path2, cwd, root) + (!path2 || path2.endsWith("/") ? "/" : "");
  }
  return paths;
}
function crawl(options, cwd, sync) {
  if (process.env.TINYGLOBBY_DEBUG) {
    options.debug = true;
  }
  if (options.debug) {
    log("globbing with options:", options, "cwd:", cwd);
  }
  if (Array.isArray(options.patterns) && options.patterns.length === 0) {
    return sync ? [] : Promise.resolve([]);
  }
  const props = {
    root: cwd,
    commonPath: null,
    depthOffset: 0
  };
  const processed = processPatterns(options, cwd, props);
  const nocase = options.caseSensitiveMatch === false;
  if (options.debug) {
    log("internal processing patterns:", processed);
  }
  const matcher = (0, import_picomatch2.default)(processed.match, {
    dot: options.dot,
    nocase,
    ignore: processed.ignore
  });
  const ignore = (0, import_picomatch2.default)(processed.ignore, {
    dot: options.dot,
    nocase
  });
  const partialMatcher = getPartialMatcher(processed.match, {
    dot: options.dot,
    nocase
  });
  const fdirOptions = {
    // use relative paths in the matcher
    filters: [
      options.debug ? (p, isDirectory) => {
        const path2 = processPath(p, cwd, props.root, isDirectory, options.absolute);
        const matches = matcher(path2);
        if (matches) {
          log(`matched ${path2}`);
        }
        return matches;
      } : (p, isDirectory) => matcher(processPath(p, cwd, props.root, isDirectory, options.absolute))
    ],
    exclude: options.debug ? (_, p) => {
      const relativePath = processPath(p, cwd, props.root, true, true);
      const skipped = relativePath !== "." && !partialMatcher(relativePath) || ignore(relativePath);
      if (skipped) {
        log(`skipped ${p}`);
      } else {
        log(`crawling ${p}`);
      }
      return skipped;
    } : (_, p) => {
      const relativePath = processPath(p, cwd, props.root, true, true);
      return relativePath !== "." && !partialMatcher(relativePath) || ignore(relativePath);
    },
    pathSeparator: "/",
    relativePaths: true,
    resolveSymlinks: true
  };
  if (options.deep) {
    fdirOptions.maxDepth = Math.round(options.deep - props.depthOffset);
  }
  if (options.absolute) {
    fdirOptions.relativePaths = false;
    fdirOptions.resolvePaths = true;
    fdirOptions.includeBasePath = true;
  }
  if (options.followSymbolicLinks === false) {
    fdirOptions.resolveSymlinks = false;
    fdirOptions.excludeSymlinks = true;
  }
  if (options.onlyDirectories) {
    fdirOptions.excludeFiles = true;
    fdirOptions.includeDirs = true;
  } else if (options.onlyFiles === false) {
    fdirOptions.includeDirs = true;
  }
  props.root = props.root.replace(BACKSLASHES, "");
  const root = props.root;
  if (options.debug) {
    log("internal properties:", props);
  }
  const api = new import_fdir.fdir(fdirOptions).crawl(root);
  if (cwd === root || options.absolute) {
    return sync ? api.sync() : api.withPromise();
  }
  return sync ? formatPaths(api.sync(), cwd, root) : api.withPromise().then((paths) => formatPaths(paths, cwd, root));
}
async function glob(patternsOrOptions, options) {
  if (patternsOrOptions && (options == null ? void 0 : options.patterns)) {
    throw new Error("Cannot pass patterns as both an argument and an option");
  }
  const opts = Array.isArray(patternsOrOptions) || typeof patternsOrOptions === "string" ? { ...options, patterns: patternsOrOptions } : patternsOrOptions;
  const cwd = opts.cwd ? import_node_path.default.resolve(opts.cwd).replace(BACKSLASHES, "/") : process.cwd().replace(BACKSLASHES, "/");
  return crawl(opts, cwd, false);
}
function globSync(patternsOrOptions, options) {
  if (patternsOrOptions && (options == null ? void 0 : options.patterns)) {
    throw new Error("Cannot pass patterns as both an argument and an option");
  }
  const opts = Array.isArray(patternsOrOptions) || typeof patternsOrOptions === "string" ? { ...options, patterns: patternsOrOptions } : patternsOrOptions;
  const cwd = opts.cwd ? import_node_path.default.resolve(opts.cwd).replace(BACKSLASHES, "/") : process.cwd().replace(BACKSLASHES, "/");
  return crawl(opts, cwd, true);
}
// Annotate the CommonJS export names for ESM import in node:
0 && (module.exports = {
  convertPathToPattern,
  escapePath,
  glob,
  globSync,
  isDynamicPattern
});
