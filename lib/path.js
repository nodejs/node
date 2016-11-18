'use strict';

const inspect = require('util').inspect;

function assertPath(path) {
  if (typeof path !== 'string') {
    throw new TypeError('Path must be a string. Received ' + inspect(path));
  }
}

// Resolves . and .. elements in a path with directory names
function normalizeStringWin32(path, allowAboveRoot) {
  var res = '';
  var lastSlash = -1;
  var dots = 0;
  var code;
  for (var i = 0; i <= path.length; ++i) {
    if (i < path.length)
      code = path.charCodeAt(i);
    else if (code === 47/*/*/ || code === 92/*\*/)
      break;
    else
      code = 47/*/*/;
    if (code === 47/*/*/ || code === 92/*\*/) {
      if (lastSlash === i - 1 || dots === 1) {
        // NOOP
      } else if (lastSlash !== i - 1 && dots === 2) {
        if (res.length < 2 ||
            res.charCodeAt(res.length - 1) !== 46/*.*/ ||
            res.charCodeAt(res.length - 2) !== 46/*.*/) {
          if (res.length > 2) {
            const start = res.length - 1;
            var j = start;
            for (; j >= 0; --j) {
              if (res.charCodeAt(j) === 92/*\*/)
                break;
            }
            if (j !== start) {
              if (j === -1)
                res = '';
              else
                res = res.slice(0, j);
              lastSlash = i;
              dots = 0;
              continue;
            }
          } else if (res.length === 2 || res.length === 1) {
            res = '';
            lastSlash = i;
            dots = 0;
            continue;
          }
        }
        if (allowAboveRoot) {
          if (res.length > 0)
            res += '\\..';
          else
            res = '..';
        }
      } else {
        if (res.length > 0)
          res += '\\' + path.slice(lastSlash + 1, i);
        else
          res = path.slice(lastSlash + 1, i);
      }
      lastSlash = i;
      dots = 0;
    } else if (code === 46/*.*/ && dots !== -1) {
      ++dots;
    } else {
      dots = -1;
    }
  }
  return res;
}

// Resolves . and .. elements in a path with directory names
function normalizeStringPosix(path, allowAboveRoot) {
  var res = '';
  var lastSlash = -1;
  var dots = 0;
  var code;
  for (var i = 0; i <= path.length; ++i) {
    if (i < path.length)
      code = path.charCodeAt(i);
    else if (code === 47/*/*/)
      break;
    else
      code = 47/*/*/;
    if (code === 47/*/*/) {
      if (lastSlash === i - 1 || dots === 1) {
        // NOOP
      } else if (lastSlash !== i - 1 && dots === 2) {
        if (res.length < 2 ||
            res.charCodeAt(res.length - 1) !== 46/*.*/ ||
            res.charCodeAt(res.length - 2) !== 46/*.*/) {
          if (res.length > 2) {
            const start = res.length - 1;
            var j = start;
            for (; j >= 0; --j) {
              if (res.charCodeAt(j) === 47/*/*/)
                break;
            }
            if (j !== start) {
              if (j === -1)
                res = '';
              else
                res = res.slice(0, j);
              lastSlash = i;
              dots = 0;
              continue;
            }
          } else if (res.length === 2 || res.length === 1) {
            res = '';
            lastSlash = i;
            dots = 0;
            continue;
          }
        }
        if (allowAboveRoot) {
          if (res.length > 0)
            res += '/..';
          else
            res = '..';
        }
      } else {
        if (res.length > 0)
          res += '/' + path.slice(lastSlash + 1, i);
        else
          res = path.slice(lastSlash + 1, i);
      }
      lastSlash = i;
      dots = 0;
    } else if (code === 46/*.*/ && dots !== -1) {
      ++dots;
    } else {
      dots = -1;
    }
  }
  return res;
}

function _format(sep, pathObject) {
  const dir = pathObject.dir || pathObject.root;
  const base = pathObject.base ||
    ((pathObject.name || '') + (pathObject.ext || ''));
  if (!dir) {
    return base;
  }
  if (dir === pathObject.root) {
    return dir + base;
  }
  return dir + sep + base;
}

const win32 = {
  // path.resolve([from ...], to)
  resolve: function resolve() {
    var resolvedDevice = '';
    var resolvedTail = '';
    var resolvedAbsolute = false;

    for (var i = arguments.length - 1; i >= -1; i--) {
      var path;
      if (i >= 0) {
        path = arguments[i];
      } else if (!resolvedDevice) {
        path = process.cwd();
      } else {
        // Windows has the concept of drive-specific current working
        // directories. If we've resolved a drive letter but not yet an
        // absolute path, get cwd for that drive, or the process cwd if
        // the drive cwd is not available. We're sure the device is not
        // a UNC path at this points, because UNC paths are always absolute.
        path = process.env['=' + resolvedDevice] || process.cwd();

        // Verify that a cwd was found and that it actually points
        // to our drive. If not, default to the drive's root.
        if (path === undefined ||
            path.slice(0, 3).toLowerCase() !==
              resolvedDevice.toLowerCase() + '\\') {
          path = resolvedDevice + '\\';
        }
      }

      assertPath(path);

      // Skip empty entries
      if (path.length === 0) {
        continue;
      }

      var len = path.length;
      var rootEnd = 0;
      var code = path.charCodeAt(0);
      var device = '';
      var isAbsolute = false;

      // Try to match a root
      if (len > 1) {
        if (code === 47/*/*/ || code === 92/*\*/) {
          // Possible UNC root

          // If we started with a separator, we know we at least have an
          // absolute path of some kind (UNC or otherwise)
          isAbsolute = true;

          code = path.charCodeAt(1);
          if (code === 47/*/*/ || code === 92/*\*/) {
            // Matched double path separator at beginning
            var j = 2;
            var last = j;
            // Match 1 or more non-path separators
            for (; j < len; ++j) {
              code = path.charCodeAt(j);
              if (code === 47/*/*/ || code === 92/*\*/)
                break;
            }
            if (j < len && j !== last) {
              const firstPart = path.slice(last, j);
              // Matched!
              last = j;
              // Match 1 or more path separators
              for (; j < len; ++j) {
                code = path.charCodeAt(j);
                if (code !== 47/*/*/ && code !== 92/*\*/)
                  break;
              }
              if (j < len && j !== last) {
                // Matched!
                last = j;
                // Match 1 or more non-path separators
                for (; j < len; ++j) {
                  code = path.charCodeAt(j);
                  if (code === 47/*/*/ || code === 92/*\*/)
                    break;
                }
                if (j === len) {
                  // We matched a UNC root only

                  device = '\\\\' + firstPart + '\\' + path.slice(last);
                  rootEnd = j;
                } else if (j !== last) {
                  // We matched a UNC root with leftovers

                  device = '\\\\' + firstPart + '\\' + path.slice(last, j);
                  rootEnd = j;
                }
              }
            }
          } else {
            rootEnd = 1;
          }
        } else if ((code >= 65/*A*/ && code <= 90/*Z*/) ||
                   (code >= 97/*a*/ && code <= 122/*z*/)) {
          // Possible device root

          code = path.charCodeAt(1);
          if (path.charCodeAt(1) === 58/*:*/) {
            device = path.slice(0, 2);
            rootEnd = 2;
            if (len > 2) {
              code = path.charCodeAt(2);
              if (code === 47/*/*/ || code === 92/*\*/) {
                // Treat separator following drive name as an absolute path
                // indicator
                isAbsolute = true;
                rootEnd = 3;
              }
            }
          }
        }
      } else if (code === 47/*/*/ || code === 92/*\*/) {
        // `path` contains just a path separator
        rootEnd = 1;
        isAbsolute = true;
      }

      if (device.length > 0 &&
          resolvedDevice.length > 0 &&
          device.toLowerCase() !== resolvedDevice.toLowerCase()) {
        // This path points to another device so it is not applicable
        continue;
      }

      if (resolvedDevice.length === 0 && device.length > 0) {
        resolvedDevice = device;
      }
      if (!resolvedAbsolute) {
        resolvedTail = path.slice(rootEnd) + '\\' + resolvedTail;
        resolvedAbsolute = isAbsolute;
      }

      if (resolvedDevice.length > 0 && resolvedAbsolute) {
        break;
      }
    }

    // At this point the path should be resolved to a full absolute path,
    // but handle relative paths to be safe (might happen when process.cwd()
    // fails)

    // Normalize the tail path
    resolvedTail = normalizeStringWin32(resolvedTail, !resolvedAbsolute);

    return (resolvedDevice + (resolvedAbsolute ? '\\' : '') + resolvedTail) ||
           '.';
  },

  normalize: function normalize(path) {
    assertPath(path);
    const len = path.length;
    if (len === 0)
      return '.';
    var rootEnd = 0;
    var code = path.charCodeAt(0);
    var device;
    var isAbsolute = false;

    // Try to match a root
    if (len > 1) {
      if (code === 47/*/*/ || code === 92/*\*/) {
        // Possible UNC root

        // If we started with a separator, we know we at least have an absolute
        // path of some kind (UNC or otherwise)
        isAbsolute = true;

        code = path.charCodeAt(1);
        if (code === 47/*/*/ || code === 92/*\*/) {
          // Matched double path separator at beginning
          var j = 2;
          var last = j;
          // Match 1 or more non-path separators
          for (; j < len; ++j) {
            code = path.charCodeAt(j);
            if (code === 47/*/*/ || code === 92/*\*/)
              break;
          }
          if (j < len && j !== last) {
            const firstPart = path.slice(last, j);
            // Matched!
            last = j;
            // Match 1 or more path separators
            for (; j < len; ++j) {
              code = path.charCodeAt(j);
              if (code !== 47/*/*/ && code !== 92/*\*/)
                break;
            }
            if (j < len && j !== last) {
              // Matched!
              last = j;
              // Match 1 or more non-path separators
              for (; j < len; ++j) {
                code = path.charCodeAt(j);
                if (code === 47/*/*/ || code === 92/*\*/)
                  break;
              }
              if (j === len) {
                // We matched a UNC root only
                // Return the normalized version of the UNC root since there
                // is nothing left to process

                return '\\\\' + firstPart + '\\' + path.slice(last) + '\\';
              } else if (j !== last) {
                // We matched a UNC root with leftovers

                device = '\\\\' + firstPart + '\\' + path.slice(last, j);
                rootEnd = j;
              }
            }
          }
        } else {
          rootEnd = 1;
        }
      } else if ((code >= 65/*A*/ && code <= 90/*Z*/) ||
                 (code >= 97/*a*/ && code <= 122/*z*/)) {
        // Possible device root

        code = path.charCodeAt(1);
        if (path.charCodeAt(1) === 58/*:*/) {
          device = path.slice(0, 2);
          rootEnd = 2;
          if (len > 2) {
            code = path.charCodeAt(2);
            if (code === 47/*/*/ || code === 92/*\*/) {
              // Treat separator following drive name as an absolute path
              // indicator
              isAbsolute = true;
              rootEnd = 3;
            }
          }
        }
      }
    } else if (code === 47/*/*/ || code === 92/*\*/) {
      // `path` contains just a path separator, exit early to avoid unnecessary
      // work
      return '\\';
    }

    code = path.charCodeAt(len - 1);
    var trailingSeparator = (code === 47/*/*/ || code === 92/*\*/);
    var tail;
    if (rootEnd < len)
      tail = normalizeStringWin32(path.slice(rootEnd), !isAbsolute);
    else
      tail = '';
    if (tail.length === 0 && !isAbsolute)
      tail = '.';
    if (tail.length > 0 && trailingSeparator)
      tail += '\\';
    if (device === undefined) {
      if (isAbsolute) {
        if (tail.length > 0)
          return '\\' + tail;
        else
          return '\\';
      } else if (tail.length > 0) {
        return tail;
      } else {
        return '';
      }
    } else {
      if (isAbsolute) {
        if (tail.length > 0)
          return device + '\\' + tail;
        else
          return device + '\\';
      } else if (tail.length > 0) {
        return device + tail;
      } else {
        return device;
      }
    }
  },


  isAbsolute: function isAbsolute(path) {
    assertPath(path);
    const len = path.length;
    if (len === 0)
      return false;
    var code = path.charCodeAt(0);
    if (code === 47/*/*/ || code === 92/*\*/) {
      return true;
    } else if ((code >= 65/*A*/ && code <= 90/*Z*/) ||
               (code >= 97/*a*/ && code <= 122/*z*/)) {
      // Possible device root

      if (len > 2 && path.charCodeAt(1) === 58/*:*/) {
        code = path.charCodeAt(2);
        if (code === 47/*/*/ || code === 92/*\*/)
          return true;
      }
    }
    return false;
  },


  join: function join() {
    if (arguments.length === 0)
      return '.';

    var joined;
    var firstPart;
    for (var i = 0; i < arguments.length; ++i) {
      var arg = arguments[i];
      assertPath(arg);
      if (arg.length > 0) {
        if (joined === undefined)
          joined = firstPart = arg;
        else
          joined += '\\' + arg;
      }
    }

    if (joined === undefined)
      return '.';

    // Make sure that the joined path doesn't start with two slashes, because
    // normalize() will mistake it for an UNC path then.
    //
    // This step is skipped when it is very clear that the user actually
    // intended to point at an UNC path. This is assumed when the first
    // non-empty string arguments starts with exactly two slashes followed by
    // at least one more non-slash character.
    //
    // Note that for normalize() to treat a path as an UNC path it needs to
    // have at least 2 components, so we don't filter for that here.
    // This means that the user can use join to construct UNC paths from
    // a server name and a share name; for example:
    //   path.join('//server', 'share') -> '\\\\server\\share\\')
    //var firstPart = paths[0];
    var needsReplace = true;
    var slashCount = 0;
    var code = firstPart.charCodeAt(0);
    if (code === 47/*/*/ || code === 92/*\*/) {
      ++slashCount;
      const firstLen = firstPart.length;
      if (firstLen > 1) {
        code = firstPart.charCodeAt(1);
        if (code === 47/*/*/ || code === 92/*\*/) {
          ++slashCount;
          if (firstLen > 2) {
            code = firstPart.charCodeAt(2);
            if (code === 47/*/*/ || code === 92/*\*/)
              ++slashCount;
            else {
              // We matched a UNC path in the first part
              needsReplace = false;
            }
          }
        }
      }
    }
    if (needsReplace) {
      // Find any more consecutive slashes we need to replace
      for (; slashCount < joined.length; ++slashCount) {
        code = joined.charCodeAt(slashCount);
        if (code !== 47/*/*/ && code !== 92/*\*/)
          break;
      }

      // Replace the slashes if needed
      if (slashCount >= 2)
        joined = '\\' + joined.slice(slashCount);
    }

    return win32.normalize(joined);
  },


  // It will solve the relative path from `from` to `to`, for instance:
  //  from = 'C:\\orandea\\test\\aaa'
  //  to = 'C:\\orandea\\impl\\bbb'
  // The output of the function should be: '..\\..\\impl\\bbb'
  relative: function relative(from, to) {
    assertPath(from);
    assertPath(to);

    if (from === to)
      return '';

    var fromOrig = win32.resolve(from);
    var toOrig = win32.resolve(to);

    if (fromOrig === toOrig)
      return '';

    from = fromOrig.toLowerCase();
    to = toOrig.toLowerCase();

    if (from === to)
      return '';

    // Trim any leading backslashes
    var fromStart = 0;
    for (; fromStart < from.length; ++fromStart) {
      if (from.charCodeAt(fromStart) !== 92/*\*/)
        break;
    }
    // Trim trailing backslashes (applicable to UNC paths only)
    var fromEnd = from.length;
    for (; fromEnd - 1 > fromStart; --fromEnd) {
      if (from.charCodeAt(fromEnd - 1) !== 92/*\*/)
        break;
    }
    var fromLen = (fromEnd - fromStart);

    // Trim any leading backslashes
    var toStart = 0;
    for (; toStart < to.length; ++toStart) {
      if (to.charCodeAt(toStart) !== 92/*\*/)
        break;
    }
    // Trim trailing backslashes (applicable to UNC paths only)
    var toEnd = to.length;
    for (; toEnd - 1 > toStart; --toEnd) {
      if (to.charCodeAt(toEnd - 1) !== 92/*\*/)
        break;
    }
    var toLen = (toEnd - toStart);

    // Compare paths to find the longest common path from root
    var length = (fromLen < toLen ? fromLen : toLen);
    var lastCommonSep = -1;
    var i = 0;
    for (; i <= length; ++i) {
      if (i === length) {
        if (toLen > length) {
          if (to.charCodeAt(toStart + i) === 92/*\*/) {
            // We get here if `from` is the exact base path for `to`.
            // For example: from='C:\\foo\\bar'; to='C:\\foo\\bar\\baz'
            return toOrig.slice(toStart + i + 1);
          } else if (i === 2) {
            // We get here if `from` is the device root.
            // For example: from='C:\\'; to='C:\\foo'
            return toOrig.slice(toStart + i);
          }
        }
        if (fromLen > length) {
          if (from.charCodeAt(fromStart + i) === 92/*\*/) {
            // We get here if `to` is the exact base path for `from`.
            // For example: from='C:\\foo\\bar'; to='C:\\foo'
            lastCommonSep = i;
          } else if (i === 2) {
            // We get here if `to` is the device root.
            // For example: from='C:\\foo\\bar'; to='C:\\'
            lastCommonSep = 3;
          }
        }
        break;
      }
      var fromCode = from.charCodeAt(fromStart + i);
      var toCode = to.charCodeAt(toStart + i);
      if (fromCode !== toCode)
        break;
      else if (fromCode === 92/*\*/)
        lastCommonSep = i;
    }

    // We found a mismatch before the first common path separator was seen, so
    // return the original `to`.
    if (i !== length && lastCommonSep === -1) {
      return toOrig;
    }

    var out = '';
    if (lastCommonSep === -1)
      lastCommonSep = 0;
    // Generate the relative path based on the path difference between `to` and
    // `from`
    for (i = fromStart + lastCommonSep + 1; i <= fromEnd; ++i) {
      if (i === fromEnd || from.charCodeAt(i) === 92/*\*/) {
        if (out.length === 0)
          out += '..';
        else
          out += '\\..';
      }
    }

    // Lastly, append the rest of the destination (`to`) path that comes after
    // the common path parts
    if (out.length > 0)
      return out + toOrig.slice(toStart + lastCommonSep, toEnd);
    else {
      toStart += lastCommonSep;
      if (toOrig.charCodeAt(toStart) === 92/*\*/)
        ++toStart;
      return toOrig.slice(toStart, toEnd);
    }
  },


  _makeLong: function _makeLong(path) {
    // Note: this will *probably* throw somewhere.
    if (typeof path !== 'string')
      return path;

    if (path.length === 0) {
      return '';
    }

    const resolvedPath = win32.resolve(path);

    if (resolvedPath.length >= 3) {
      var code = resolvedPath.charCodeAt(0);
      if (code === 92/*\*/) {
        // Possible UNC root

        if (resolvedPath.charCodeAt(1) === 92/*\*/) {
          code = resolvedPath.charCodeAt(2);
          if (code !== 63/*?*/ && code !== 46/*.*/) {
            // Matched non-long UNC root, convert the path to a long UNC path
            return '\\\\?\\UNC\\' + resolvedPath.slice(2);
          }
        }
      } else if ((code >= 65/*A*/ && code <= 90/*Z*/) ||
                 (code >= 97/*a*/ && code <= 122/*z*/)) {
        // Possible device root

        if (resolvedPath.charCodeAt(1) === 58/*:*/ &&
            resolvedPath.charCodeAt(2) === 92/*\*/) {
          // Matched device root, convert the path to a long UNC path
          return '\\\\?\\' + resolvedPath;
        }
      }
    }

    return path;
  },


  dirname: function dirname(path) {
    assertPath(path);
    const len = path.length;
    if (len === 0)
      return '.';
    var rootEnd = -1;
    var end = -1;
    var matchedSlash = true;
    var offset = 0;
    var code = path.charCodeAt(0);

    // Try to match a root
    if (len > 1) {
      if (code === 47/*/*/ || code === 92/*\*/) {
        // Possible UNC root

        rootEnd = offset = 1;

        code = path.charCodeAt(1);
        if (code === 47/*/*/ || code === 92/*\*/) {
          // Matched double path separator at beginning
          var j = 2;
          var last = j;
          // Match 1 or more non-path separators
          for (; j < len; ++j) {
            code = path.charCodeAt(j);
            if (code === 47/*/*/ || code === 92/*\*/)
              break;
          }
          if (j < len && j !== last) {
            // Matched!
            last = j;
            // Match 1 or more path separators
            for (; j < len; ++j) {
              code = path.charCodeAt(j);
              if (code !== 47/*/*/ && code !== 92/*\*/)
                break;
            }
            if (j < len && j !== last) {
              // Matched!
              last = j;
              // Match 1 or more non-path separators
              for (; j < len; ++j) {
                code = path.charCodeAt(j);
                if (code === 47/*/*/ || code === 92/*\*/)
                  break;
              }
              if (j === len) {
                // We matched a UNC root only
                return path;
              }
              if (j !== last) {
                // We matched a UNC root with leftovers

                // Offset by 1 to include the separator after the UNC root to
                // treat it as a "normal root" on top of a (UNC) root
                rootEnd = offset = j + 1;
              }
            }
          }
        }
      } else if ((code >= 65/*A*/ && code <= 90/*Z*/) ||
                 (code >= 97/*a*/ && code <= 122/*z*/)) {
        // Possible device root

        code = path.charCodeAt(1);
        if (path.charCodeAt(1) === 58/*:*/) {
          rootEnd = offset = 2;
          if (len > 2) {
            code = path.charCodeAt(2);
            if (code === 47/*/*/ || code === 92/*\*/)
              rootEnd = offset = 3;
          }
        }
      }
    } else if (code === 47/*/*/ || code === 92/*\*/) {
      return path[0];
    }

    for (var i = len - 1; i >= offset; --i) {
      code = path.charCodeAt(i);
      if (code === 47/*/*/ || code === 92/*\*/) {
        if (!matchedSlash) {
          end = i;
          break;
        }
      } else {
        // We saw the first non-path separator
        matchedSlash = false;
      }
    }

    if (end === -1) {
      if (rootEnd === -1)
        return '.';
      else
        end = rootEnd;
    }
    return path.slice(0, end);
  },


  basename: function basename(path, ext) {
    if (ext !== undefined && typeof ext !== 'string')
      throw new TypeError('"ext" argument must be a string');
    assertPath(path);
    var start = 0;
    var end = -1;
    var matchedSlash = true;
    var i;

    // Check for a drive letter prefix so as not to mistake the following
    // path separator as an extra separator at the end of the path that can be
    // disregarded
    if (path.length >= 2) {
      const drive = path.charCodeAt(0);
      if ((drive >= 65/*A*/ && drive <= 90/*Z*/) ||
          (drive >= 97/*a*/ && drive <= 122/*z*/)) {
        if (path.charCodeAt(1) === 58/*:*/)
          start = 2;
      }
    }

    if (ext !== undefined && ext.length > 0 && ext.length <= path.length) {
      if (ext.length === path.length && ext === path)
        return '';
      var extIdx = ext.length - 1;
      var firstNonSlashEnd = -1;
      for (i = path.length - 1; i >= start; --i) {
        const code = path.charCodeAt(i);
        if (code === 47/*/*/ || code === 92/*\*/) {
          // If we reached a path separator that was not part of a set of path
          // separators at the end of the string, stop now
          if (!matchedSlash) {
            start = i + 1;
            break;
          }
        } else {
          if (firstNonSlashEnd === -1) {
            // We saw the first non-path separator, remember this index in case
            // we need it if the extension ends up not matching
            matchedSlash = false;
            firstNonSlashEnd = i + 1;
          }
          if (extIdx >= 0) {
            // Try to match the explicit extension
            if (code === ext.charCodeAt(extIdx)) {
              if (--extIdx === -1) {
                // We matched the extension, so mark this as the end of our path
                // component
                end = i;
              }
            } else {
              // Extension does not match, so our result is the entire path
              // component
              extIdx = -1;
              end = firstNonSlashEnd;
            }
          }
        }
      }

      if (start === end)
        end = firstNonSlashEnd;
      else if (end === -1)
        end = path.length;
      return path.slice(start, end);
    } else {
      for (i = path.length - 1; i >= start; --i) {
        const code = path.charCodeAt(i);
        if (code === 47/*/*/ || code === 92/*\*/) {
          // If we reached a path separator that was not part of a set of path
          // separators at the end of the string, stop now
          if (!matchedSlash) {
            start = i + 1;
            break;
          }
        } else if (end === -1) {
          // We saw the first non-path separator, mark this as the end of our
          // path component
          matchedSlash = false;
          end = i + 1;
        }
      }

      if (end === -1)
        return '';
      return path.slice(start, end);
    }
  },


  extname: function extname(path) {
    assertPath(path);
    var startDot = -1;
    var startPart = 0;
    var end = -1;
    var matchedSlash = true;
    // Track the state of characters (if any) we see before our first dot and
    // after any path separator we find
    var preDotState = 0;
    for (var i = path.length - 1; i >= 0; --i) {
      const code = path.charCodeAt(i);
      if (code === 47/*/*/ || code === 92/*\*/) {
        // If we reached a path separator that was not part of a set of path
        // separators at the end of the string, stop now
        if (!matchedSlash) {
          startPart = i + 1;
          break;
        }
        continue;
      }
      if (end === -1) {
        // We saw the first non-path separator, mark this as the end of our
        // extension
        matchedSlash = false;
        end = i + 1;
      }
      if (code === 46/*.*/) {
        // If this is our first dot, mark it as the start of our extension
        if (startDot === -1)
          startDot = i;
        else if (preDotState !== 1)
          preDotState = 1;
      } else if (startDot !== -1) {
        // We saw a non-dot and non-path separator before our dot, so we should
        // have a good chance at having a non-empty extension
        preDotState = -1;
      }
    }

    if (startDot === -1 ||
        end === -1 ||
        // We saw a non-dot character immediately before the dot
        preDotState === 0 ||
        // The (right-most) trimmed path component is exactly '..'
        (preDotState === 1 &&
         startDot === end - 1 &&
         startDot === startPart + 1)) {
      return '';
    }
    return path.slice(startDot, end);
  },


  format: function format(pathObject) {
    if (pathObject === null || typeof pathObject !== 'object') {
      throw new TypeError(
        `Parameter "pathObject" must be an object, not ${typeof pathObject}`
      );
    }
    return _format('\\', pathObject);
  },


  parse: function parse(path) {
    assertPath(path);

    var ret = { root: '', dir: '', base: '', ext: '', name: '' };
    if (path.length === 0)
      return ret;

    var len = path.length;
    var rootEnd = 0;
    var code = path.charCodeAt(0);
    var isAbsolute = false;

    // Try to match a root
    if (len > 1) {
      if (code === 47/*/*/ || code === 92/*\*/) {
        // Possible UNC root

        isAbsolute = true;

        code = path.charCodeAt(1);
        rootEnd = 1;
        if (code === 47/*/*/ || code === 92/*\*/) {
          // Matched double path separator at beginning
          var j = 2;
          var last = j;
          // Match 1 or more non-path separators
          for (; j < len; ++j) {
            code = path.charCodeAt(j);
            if (code === 47/*/*/ || code === 92/*\*/)
              break;
          }
          if (j < len && j !== last) {
            // Matched!
            last = j;
            // Match 1 or more path separators
            for (; j < len; ++j) {
              code = path.charCodeAt(j);
              if (code !== 47/*/*/ && code !== 92/*\*/)
                break;
            }
            if (j < len && j !== last) {
              // Matched!
              last = j;
              // Match 1 or more non-path separators
              for (; j < len; ++j) {
                code = path.charCodeAt(j);
                if (code === 47/*/*/ || code === 92/*\*/)
                  break;
              }
              if (j === len) {
                // We matched a UNC root only

                rootEnd = j;
              } else if (j !== last) {
                // We matched a UNC root with leftovers

                rootEnd = j + 1;
              }
            }
          }
        }
      } else if ((code >= 65/*A*/ && code <= 90/*Z*/) ||
                 (code >= 97/*a*/ && code <= 122/*z*/)) {
        // Possible device root

        code = path.charCodeAt(1);
        if (path.charCodeAt(1) === 58/*:*/) {
          rootEnd = 2;
          if (len > 2) {
            code = path.charCodeAt(2);
            if (code === 47/*/*/ || code === 92/*\*/) {
              if (len === 3) {
                // `path` contains just a drive root, exit early to avoid
                // unnecessary work
                ret.root = ret.dir = path.slice(0, 3);
                return ret;
              }
              isAbsolute = true;
              rootEnd = 3;
            }
          } else {
            // `path` contains just a drive root, exit early to avoid
            // unnecessary work
            ret.root = ret.dir = path.slice(0, 2);
            return ret;
          }
        }
      }
    } else if (code === 47/*/*/ || code === 92/*\*/) {
      // `path` contains just a path separator, exit early to avoid
      // unnecessary work
      ret.root = ret.dir = path[0];
      return ret;
    }

    if (rootEnd > 0)
      ret.root = path.slice(0, rootEnd);

    var startDot = -1;
    var startPart = 0;
    var end = -1;
    var matchedSlash = true;
    var i = path.length - 1;

    // Track the state of characters (if any) we see before our first dot and
    // after any path separator we find
    var preDotState = 0;

    // Get non-dir info
    for (; i >= rootEnd; --i) {
      code = path.charCodeAt(i);
      if (code === 47/*/*/ || code === 92/*\*/) {
        // If we reached a path separator that was not part of a set of path
        // separators at the end of the string, stop now
        if (!matchedSlash) {
          startPart = i + 1;
          break;
        }
        continue;
      }
      if (end === -1) {
        // We saw the first non-path separator, mark this as the end of our
        // extension
        matchedSlash = false;
        end = i + 1;
      }
      if (code === 46/*.*/) {
        // If this is our first dot, mark it as the start of our extension
        if (startDot === -1)
          startDot = i;
        else if (preDotState !== 1)
          preDotState = 1;
      } else if (startDot !== -1) {
        // We saw a non-dot and non-path separator before our dot, so we should
        // have a good chance at having a non-empty extension
        preDotState = -1;
      }
    }

    if (startDot === -1 ||
        end === -1 ||
        // We saw a non-dot character immediately before the dot
        preDotState === 0 ||
        // The (right-most) trimmed path component is exactly '..'
        (preDotState === 1 &&
         startDot === end - 1 &&
         startDot === startPart + 1)) {
      if (end !== -1) {
        if (startPart === 0 && isAbsolute)
          ret.base = ret.name = path.slice(rootEnd, end);
        else
          ret.base = ret.name = path.slice(startPart, end);
      }
    } else {
      if (startPart === 0 && isAbsolute) {
        ret.name = path.slice(rootEnd, startDot);
        ret.base = path.slice(rootEnd, end);
      } else {
        ret.name = path.slice(startPart, startDot);
        ret.base = path.slice(startPart, end);
      }
      ret.ext = path.slice(startDot, end);
    }

    if (startPart > 0)
      ret.dir = path.slice(0, startPart - 1);
    else if (isAbsolute)
      ret.dir = path.slice(0, rootEnd);

    return ret;
  },


  sep: '\\',
  delimiter: ';',
  win32: null,
  posix: null
};


const posix = {
  // path.resolve([from ...], to)
  resolve: function resolve() {
    var resolvedPath = '';
    var resolvedAbsolute = false;
    var cwd;

    for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
      var path;
      if (i >= 0)
        path = arguments[i];
      else {
        if (cwd === undefined)
          cwd = process.cwd();
        path = cwd;
      }

      assertPath(path);

      // Skip empty entries
      if (path.length === 0) {
        continue;
      }

      resolvedPath = path + '/' + resolvedPath;
      resolvedAbsolute = path.charCodeAt(0) === 47/*/*/;
    }

    // At this point the path should be resolved to a full absolute path, but
    // handle relative paths to be safe (might happen when process.cwd() fails)

    // Normalize the path
    resolvedPath = normalizeStringPosix(resolvedPath, !resolvedAbsolute);

    if (resolvedAbsolute) {
      if (resolvedPath.length > 0)
        return '/' + resolvedPath;
      else
        return '/';
    } else if (resolvedPath.length > 0) {
      return resolvedPath;
    } else {
      return '.';
    }
  },


  normalize: function normalize(path) {
    assertPath(path);

    if (path.length === 0)
      return '.';

    const isAbsolute = path.charCodeAt(0) === 47/*/*/;
    const trailingSeparator = path.charCodeAt(path.length - 1) === 47/*/*/;

    // Normalize the path
    path = normalizeStringPosix(path, !isAbsolute);

    if (path.length === 0 && !isAbsolute)
      path = '.';
    if (path.length > 0 && trailingSeparator)
      path += '/';

    if (isAbsolute)
      return '/' + path;
    return path;
  },


  isAbsolute: function isAbsolute(path) {
    assertPath(path);
    return path.length > 0 && path.charCodeAt(0) === 47/*/*/;
  },


  join: function join() {
    if (arguments.length === 0)
      return '.';
    var joined;
    for (var i = 0; i < arguments.length; ++i) {
      var arg = arguments[i];
      assertPath(arg);
      if (arg.length > 0) {
        if (joined === undefined)
          joined = arg;
        else
          joined += '/' + arg;
      }
    }
    if (joined === undefined)
      return '.';
    return posix.normalize(joined);
  },


  relative: function relative(from, to) {
    assertPath(from);
    assertPath(to);

    if (from === to)
      return '';

    from = posix.resolve(from);
    to = posix.resolve(to);

    if (from === to)
      return '';

    // Trim any leading backslashes
    var fromStart = 1;
    for (; fromStart < from.length; ++fromStart) {
      if (from.charCodeAt(fromStart) !== 47/*/*/)
        break;
    }
    var fromEnd = from.length;
    var fromLen = (fromEnd - fromStart);

    // Trim any leading backslashes
    var toStart = 1;
    for (; toStart < to.length; ++toStart) {
      if (to.charCodeAt(toStart) !== 47/*/*/)
        break;
    }
    var toEnd = to.length;
    var toLen = (toEnd - toStart);

    // Compare paths to find the longest common path from root
    var length = (fromLen < toLen ? fromLen : toLen);
    var lastCommonSep = -1;
    var i = 0;
    for (; i <= length; ++i) {
      if (i === length) {
        if (toLen > length) {
          if (to.charCodeAt(toStart + i) === 47/*/*/) {
            // We get here if `from` is the exact base path for `to`.
            // For example: from='/foo/bar'; to='/foo/bar/baz'
            return to.slice(toStart + i + 1);
          } else if (i === 0) {
            // We get here if `from` is the root
            // For example: from='/'; to='/foo'
            return to.slice(toStart + i);
          }
        } else if (fromLen > length) {
          if (from.charCodeAt(fromStart + i) === 47/*/*/) {
            // We get here if `to` is the exact base path for `from`.
            // For example: from='/foo/bar/baz'; to='/foo/bar'
            lastCommonSep = i;
          } else if (i === 0) {
            // We get here if `to` is the root.
            // For example: from='/foo'; to='/'
            lastCommonSep = 0;
          }
        }
        break;
      }
      var fromCode = from.charCodeAt(fromStart + i);
      var toCode = to.charCodeAt(toStart + i);
      if (fromCode !== toCode)
        break;
      else if (fromCode === 47/*/*/)
        lastCommonSep = i;
    }

    var out = '';
    // Generate the relative path based on the path difference between `to`
    // and `from`
    for (i = fromStart + lastCommonSep + 1; i <= fromEnd; ++i) {
      if (i === fromEnd || from.charCodeAt(i) === 47/*/*/) {
        if (out.length === 0)
          out += '..';
        else
          out += '/..';
      }
    }

    // Lastly, append the rest of the destination (`to`) path that comes after
    // the common path parts
    if (out.length > 0)
      return out + to.slice(toStart + lastCommonSep);
    else {
      toStart += lastCommonSep;
      if (to.charCodeAt(toStart) === 47/*/*/)
        ++toStart;
      return to.slice(toStart);
    }
  },


  _makeLong: function _makeLong(path) {
    return path;
  },


  dirname: function dirname(path) {
    assertPath(path);
    if (path.length === 0)
      return '.';
    var code = path.charCodeAt(0);
    var hasRoot = (code === 47/*/*/);
    var end = -1;
    var matchedSlash = true;
    for (var i = path.length - 1; i >= 1; --i) {
      code = path.charCodeAt(i);
      if (code === 47/*/*/) {
        if (!matchedSlash) {
          end = i;
          break;
        }
      } else {
        // We saw the first non-path separator
        matchedSlash = false;
      }
    }

    if (end === -1)
      return hasRoot ? '/' : '.';
    if (hasRoot && end === 1)
      return '//';
    return path.slice(0, end);
  },


  basename: function basename(path, ext) {
    if (ext !== undefined && typeof ext !== 'string')
      throw new TypeError('"ext" argument must be a string');
    assertPath(path);

    var start = 0;
    var end = -1;
    var matchedSlash = true;
    var i;

    if (ext !== undefined && ext.length > 0 && ext.length <= path.length) {
      if (ext.length === path.length && ext === path)
        return '';
      var extIdx = ext.length - 1;
      var firstNonSlashEnd = -1;
      for (i = path.length - 1; i >= 0; --i) {
        const code = path.charCodeAt(i);
        if (code === 47/*/*/) {
          // If we reached a path separator that was not part of a set of path
          // separators at the end of the string, stop now
          if (!matchedSlash) {
            start = i + 1;
            break;
          }
        } else {
          if (firstNonSlashEnd === -1) {
            // We saw the first non-path separator, remember this index in case
            // we need it if the extension ends up not matching
            matchedSlash = false;
            firstNonSlashEnd = i + 1;
          }
          if (extIdx >= 0) {
            // Try to match the explicit extension
            if (code === ext.charCodeAt(extIdx)) {
              if (--extIdx === -1) {
                // We matched the extension, so mark this as the end of our path
                // component
                end = i;
              }
            } else {
              // Extension does not match, so our result is the entire path
              // component
              extIdx = -1;
              end = firstNonSlashEnd;
            }
          }
        }
      }

      if (start === end)
        end = firstNonSlashEnd;
      else if (end === -1)
        end = path.length;
      return path.slice(start, end);
    } else {
      for (i = path.length - 1; i >= 0; --i) {
        if (path.charCodeAt(i) === 47/*/*/) {
          // If we reached a path separator that was not part of a set of path
          // separators at the end of the string, stop now
          if (!matchedSlash) {
            start = i + 1;
            break;
          }
        } else if (end === -1) {
          // We saw the first non-path separator, mark this as the end of our
          // path component
          matchedSlash = false;
          end = i + 1;
        }
      }

      if (end === -1)
        return '';
      return path.slice(start, end);
    }
  },


  extname: function extname(path) {
    assertPath(path);
    var startDot = -1;
    var startPart = 0;
    var end = -1;
    var matchedSlash = true;
    // Track the state of characters (if any) we see before our first dot and
    // after any path separator we find
    var preDotState = 0;
    for (var i = path.length - 1; i >= 0; --i) {
      const code = path.charCodeAt(i);
      if (code === 47/*/*/) {
        // If we reached a path separator that was not part of a set of path
        // separators at the end of the string, stop now
        if (!matchedSlash) {
          startPart = i + 1;
          break;
        }
        continue;
      }
      if (end === -1) {
        // We saw the first non-path separator, mark this as the end of our
        // extension
        matchedSlash = false;
        end = i + 1;
      }
      if (code === 46/*.*/) {
        // If this is our first dot, mark it as the start of our extension
        if (startDot === -1)
          startDot = i;
        else if (preDotState !== 1)
          preDotState = 1;
      } else if (startDot !== -1) {
        // We saw a non-dot and non-path separator before our dot, so we should
        // have a good chance at having a non-empty extension
        preDotState = -1;
      }
    }

    if (startDot === -1 ||
        end === -1 ||
        // We saw a non-dot character immediately before the dot
        preDotState === 0 ||
        // The (right-most) trimmed path component is exactly '..'
        (preDotState === 1 &&
         startDot === end - 1 &&
         startDot === startPart + 1)) {
      return '';
    }
    return path.slice(startDot, end);
  },


  format: function format(pathObject) {
    if (pathObject === null || typeof pathObject !== 'object') {
      throw new TypeError(
        `Parameter "pathObject" must be an object, not ${typeof pathObject}`
      );
    }
    return _format('/', pathObject);
  },


  parse: function parse(path) {
    assertPath(path);

    var ret = { root: '', dir: '', base: '', ext: '', name: '' };
    if (path.length === 0)
      return ret;
    var code = path.charCodeAt(0);
    var isAbsolute = (code === 47/*/*/);
    var start;
    if (isAbsolute) {
      ret.root = '/';
      start = 1;
    } else {
      start = 0;
    }
    var startDot = -1;
    var startPart = 0;
    var end = -1;
    var matchedSlash = true;
    var i = path.length - 1;

    // Track the state of characters (if any) we see before our first dot and
    // after any path separator we find
    var preDotState = 0;

    // Get non-dir info
    for (; i >= start; --i) {
      code = path.charCodeAt(i);
      if (code === 47/*/*/) {
        // If we reached a path separator that was not part of a set of path
        // separators at the end of the string, stop now
        if (!matchedSlash) {
          startPart = i + 1;
          break;
        }
        continue;
      }
      if (end === -1) {
        // We saw the first non-path separator, mark this as the end of our
        // extension
        matchedSlash = false;
        end = i + 1;
      }
      if (code === 46/*.*/) {
        // If this is our first dot, mark it as the start of our extension
        if (startDot === -1)
          startDot = i;
        else if (preDotState !== 1)
          preDotState = 1;
      } else if (startDot !== -1) {
        // We saw a non-dot and non-path separator before our dot, so we should
        // have a good chance at having a non-empty extension
        preDotState = -1;
      }
    }

    if (startDot === -1 ||
        end === -1 ||
        // We saw a non-dot character immediately before the dot
        preDotState === 0 ||
        // The (right-most) trimmed path component is exactly '..'
        (preDotState === 1 &&
         startDot === end - 1 &&
         startDot === startPart + 1)) {
      if (end !== -1) {
        if (startPart === 0 && isAbsolute)
          ret.base = ret.name = path.slice(1, end);
        else
          ret.base = ret.name = path.slice(startPart, end);
      }
    } else {
      if (startPart === 0 && isAbsolute) {
        ret.name = path.slice(1, startDot);
        ret.base = path.slice(1, end);
      } else {
        ret.name = path.slice(startPart, startDot);
        ret.base = path.slice(startPart, end);
      }
      ret.ext = path.slice(startDot, end);
    }

    if (startPart > 0)
      ret.dir = path.slice(0, startPart - 1);
    else if (isAbsolute)
      ret.dir = '/';

    return ret;
  },


  sep: '/',
  delimiter: ':',
  win32: null,
  posix: null
};


posix.win32 = win32.win32 = win32;
posix.posix = win32.posix = posix;


if (process.platform === 'win32')
  module.exports = win32;
else
  module.exports = posix;
