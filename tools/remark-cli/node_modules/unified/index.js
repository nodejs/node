/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module unified
 * @fileoverview Pluggable text processing interface.
 */

'use strict';

/* Dependencies. */
var events = require('events');
var has = require('has');
var once = require('once');
var extend = require('extend');
var bail = require('bail');
var vfile = require('vfile');
var trough = require('trough');
var buffer = require('is-buffer');
var string = require('x-is-string');

/* Expose an abstract processor. */
module.exports = unified().abstract();

/* Methods. */
var slice = [].slice;

/* Process pipeline. */
var pipeline = trough()
  .use(function (p, ctx) {
    ctx.tree = p.parse(ctx.file, ctx.options);
  })
  .use(function (p, ctx, next) {
    p.run(ctx.tree, ctx.file, function (err, tree, file) {
      if (err) {
        next(err);
      } else {
        ctx.tree = tree;
        ctx.file = file;
        next();
      }
    });
  })
  .use(function (p, ctx) {
    ctx.file.contents = p.stringify(ctx.tree, ctx.file, ctx.options);
  });

/**
 * Function to create the first processor.
 *
 * @return {Function} - First processor.
 */
function unified() {
  var attachers = [];
  var transformers = trough();
  var namespace = {};
  var chunks = [];
  var emitter = new events.EventEmitter();
  var ended = false;
  var concrete = true;
  var settings;
  var key;

  /* Mix in methods. */
  for (key in emitter) {
    processor[key] = emitter[key];
  }

  /* Throw as early as possible.
   * As events are triggered synchroneously, the stack
   * is preserved. */
  processor.on('pipe', function () {
    assertConcrete();
  });

  /* Data management. */
  processor.data = data;

  /* Lock. */
  processor.abstract = abstract;

  /* Plug-ins. */
  processor.use = use;

  /* Streaming. */
  processor.writable = true;
  processor.readable = true;
  processor.write = write;
  processor.end = end;
  processor.pipe = pipe;

  /* API. */
  processor.parse = parse;
  processor.stringify = stringify;
  processor.run = run;
  processor.process = process;

  /* Expose. */
  return processor;

  /**
   * Create a new processor based on the processor
   * in the current scope.
   *
   * @return {Processor} - New concrete processor based
   *   on the descendant processor.
   */
  function processor() {
    var destination = unified();
    var length = attachers.length;
    var index = -1;

    while (++index < length) {
      destination.use.apply(null, attachers[index]);
    }

    destination.data(extend(true, {}, namespace));

    return destination;
  }

  /* Helpers. */

  /**
   * Assert a parser is available.
   *
   * @param {string} name - Name of callee.
   */
  function assertParser(name) {
    if (!isParser(processor.Parser)) {
      throw new Error('Cannot `' + name + '` without `Parser`');
    }
  }

  /**
   * Assert a compiler is available.
   *
   * @param {string} name - Name of callee.
   */
  function assertCompiler(name) {
    if (!isCompiler(processor.Compiler)) {
      throw new Error('Cannot `' + name + '` without `Compiler`');
    }
  }

  /**
   * Assert the processor is concrete.
   *
   * @param {string} name - Name of callee.
   */
  function assertConcrete(name) {
    if (!concrete) {
      throw new Error(
        'Cannot ' +
        (name ? 'invoke `' + name + '` on' : 'pipe into') +
        ' abstract processor.\n' +
        'To make the processor concrete, invoke it: ' +
        'use `processor()` instead of `processor`.'
      );
    }
  }

  /**
   * Assert `node` is a Unist node.
   *
   * @param {*} node - Value to check.
   */
  function assertNode(node) {
    if (!isNode(node)) {
      throw new Error('Expected node, got `' + node + '`');
    }
  }

  /**
   * Assert, if no `done` is given, that `complete` is
   * `true`.
   *
   * @param {string} name - Name of callee.
   * @param {boolean} complete - Whether an async process
   *   is complete.
   * @param {Function?} done - Optional handler of async
   *   results.
   */
  function assertDone(name, complete, done) {
    if (!complete && !done) {
      throw new Error(
        'Expected `done` to be given to `' + name + '` ' +
        'as async plug-ins are used'
      );
    }
  }

  /**
   * Abstract: used to signal an abstract processor which
   * should made concrete before using.
   *
   * For example, take unified itself.  It’s abstract.
   * Plug-ins should not be added to it.  Rather, it should
   * be made concrete (by invoking it) before modifying it.
   *
   * In essence, always invoke this when exporting a
   * processor.
   *
   * @return {Processor} - The operated on processor.
   */
  function abstract() {
    concrete = false;

    return processor;
  }

  /**
   * Data management.
   *
   * Getter / setter for processor-specific informtion.
   *
   * @param {string} key - Key to get or set.
   * @param {*} value - Value to set.
   * @return {*} - Either the operator on processor in
   *   setter mode; or the value stored as `key` in
   *   getter mode.
   */
  function data(key, value) {
    assertConcrete('data');

    if (string(key)) {
      /* Set `key`. */
      if (arguments.length === 2) {
        namespace[key] = value;

        return processor;
      }

      /* Get `key`. */
      return (has(namespace, key) && namespace[key]) || null;
    }

    /* Get space. */
    if (!key) {
      return namespace;
    }

    /* Set space. */
    namespace = key;

    return processor;
  }

  /**
   * Plug-in management.
   *
   * Pass it:
   * *   an attacher and options,
   * *   a list of attachers and options for all of them;
   * *   a tuple of one attacher and options.
   * *   a matrix: list containing any of the above and
   *     matrices.
   *
   * @param {...*} value - See description.
   * @return {Processor} - The operated on processor.
   */
  function use(value) {
    var args = slice.call(arguments, 0);
    var params = args.slice(1);
    var index;
    var length;
    var transformer;

    assertConcrete('use');

    /* Multiple attachers. */
    if ('length' in value && !isFunction(value)) {
      index = -1;
      length = value.length;

      if (!isFunction(value[0])) {
        /* Matrix of things. */
        while (++index < length) {
          use(value[index]);
        }
      } else if (isFunction(value[1])) {
        /* List of things. */
        while (++index < length) {
          use.apply(null, [value[index]].concat(params));
        }
      } else {
        /* Arguments. */
        use.apply(null, value);
      }

      return processor;
    }

    /* Store attacher. */
    attachers.push(args);

    /* Single attacher. */
    transformer = value.apply(null, [processor].concat(params));

    if (isFunction(transformer)) {
      transformers.use(transformer);
    }

    return processor;
  }

  /**
   * Parse a file (in string or VFile representation)
   * into a Unist node using the `Parser` on the
   * processor.
   *
   * @param {VFile?} [file] - File to process.
   * @param {Object?} [options] - Configuration.
   * @return {Node} - Unist node.
   */
  function parse(file, options) {
    assertConcrete('parse');
    assertParser('parse');

    return new processor.Parser(vfile(file), options, processor).parse();
  }

  /**
   * Run transforms on a Unist node representation of a file
   * (in string or VFile representation).
   *
   * @param {Node} node - Unist node.
   * @param {(string|VFile)?} [file] - File representation.
   * @param {Function?} [done] - Callback.
   * @return {Node} - The given or resulting Unist node.
   */
  function run(node, file, done) {
    var complete = false;
    var result;

    assertConcrete('run');
    assertNode(node);

    result = node;

    if (!done && isFunction(file)) {
      done = file;
      file = null;
    }

    transformers.run(node, vfile(file), function (err, tree, file) {
      complete = true;
      result = tree || node;

      (done || bail)(err, tree, file);
    });

    assertDone('run', complete, done);

    return result;
  }

  /**
   * Stringify a Unist node representation of a file
   * (in string or VFile representation) into a string
   * using the `Compiler` on the processor.
   *
   * @param {Node} node - Unist node.
   * @param {(string|VFile)?} [file] - File representation.
   * @param {Object?} [options] - Configuration.
   * @return {string} - String representation.
   */
  function stringify(node, file, options) {
    assertConcrete('stringify');
    assertCompiler('stringify');
    assertNode(node);

    if (
      !options &&
      !string(file) &&
      !buffer(file) &&
      !(typeof file === 'object' && 'messages' in file)
    ) {
      options = file;
      file = null;
    }

    return new processor.Compiler(vfile(file), options, processor).compile(node);
  }

  /**
   * Parse a file (in string or VFile representation)
   * into a Unist node using the `Parser` on the processor,
   * then run transforms on that node, and compile the
   * resulting node using the `Compiler` on the processor,
   * and store that result on the VFile.
   *
   * @param {(string|VFile)?} file - File representation.
   * @param {Object?} [options] - Configuration.
   * @param {Function?} [done] - Callback.
   * @return {VFile} - The given or resulting VFile.
   */
  function process(file, options, done) {
    var complete = false;

    assertConcrete('process');
    assertParser('process');
    assertCompiler('process');

    if (!done && isFunction(options)) {
      done = options;
      options = null;
    }

    file = vfile(file);

    pipeline.run(processor, {
      file: file,
      options: options || {}
    }, function (err) {
      complete = true;

      if (done) {
        done(err, file);
      } else {
        bail(err);
      }
    });

    assertDone('process', complete, done);

    return file;
  }

  /* Streams. */

  /**
   * Write a chunk into memory.
   *
   * @param {(Buffer|string)?} chunk - Value to write.
   * @param {string?} [encoding] - Encoding.
   * @param {Function?} [callback] - Callback.
   * @return {boolean} - Whether the write was succesful.
   */
  function write(chunk, encoding, callback) {
    assertConcrete('write');

    if (isFunction(encoding)) {
      callback = encoding;
      encoding = null;
    }

    if (ended) {
      throw new Error('Did not expect `write` after `end`');
    }

    chunks.push((chunk || '').toString(encoding || 'utf8'));

    if (callback) {
      callback();
    }

    /* Signal succesful write. */
    return true;
  }

  /**
   * End the writing.  Passes all arguments to a final
   * `write`.  Starts the process, which will trigger
   * `error`, with a fatal error, if any; `data`, with
   * the generated document in `string` form, if
   * succesful.  If messages are triggered during the
   * process, those are triggerd as `warning`s.
   *
   * @return {boolean} - Whether the last write was
   *   succesful.
   */
  function end() {
    assertConcrete('end');
    assertParser('end');
    assertCompiler('end');

    write.apply(null, arguments);

    ended = true;

    process(chunks.join(''), settings, function (err, file) {
      var messages = file.messages;
      var length = messages.length;
      var index = -1;

      chunks = settings = null;

      /* Trigger messages as warnings, except for fatal error. */
      while (++index < length) {
        if (messages[index] !== err) {
          processor.emit('warning', messages[index]);
        }
      }

      if (err) {
        /* Don’t enter an infinite error throwing loop. */
        global.setTimeout(function () {
          processor.emit('error', err);
        }, 4);
      } else {
        processor.emit('data', file.contents);
        processor.emit('end');
      }
    });

    return true;
  }

  /**
   * Pipe the processor into a writable stream.
   *
   * Basically `Stream#pipe`, but inlined and
   * simplified to keep the bundled size down.
   *
   * @see https://github.com/nodejs/node/blob/master/lib/stream.js#L26
   *
   * @param {Stream} dest - Writable stream.
   * @param {Object?} [options] - Processing
   *   configuration.
   * @return {Stream} - The destination stream.
   */
  function pipe(dest, options) {
    var onend = once(onended);

    assertConcrete('pipe');

    settings = options || {};

    processor.on('data', ondata);
    processor.on('error', onerror);
    processor.on('end', cleanup);
    processor.on('close', cleanup);

    /* If the 'end' option is not supplied, dest.end() will be
     * called when the 'end' or 'close' events are received.
     * Only dest.end() once. */
    if (!dest._isStdio && settings.end !== false) {
      processor.on('end', onend);
    }

    dest.on('error', onerror);
    dest.on('close', cleanup);

    dest.emit('pipe', processor);

    return dest;

    /** End destination. */
    function onended() {
      if (dest.end) {
        dest.end();
      }
    }

    /**
     * Handle data.
     *
     * @param {*} chunk - Data to pass through.
     */
    function ondata(chunk) {
      if (dest.writable) {
        dest.write(chunk);
      }
    }

    /**
     * Clean listeners.
     */
    function cleanup() {
      processor.removeListener('data', ondata);
      processor.removeListener('end', onend);
      processor.removeListener('error', onerror);
      processor.removeListener('end', cleanup);
      processor.removeListener('close', cleanup);

      dest.removeListener('error', onerror);
      dest.removeListener('close', cleanup);
    }

    /**
     * Close dangling pipes and handle unheard errors.
     *
     * @param {Error} err - Exception.
     */
    function onerror(err) {
      var handlers = processor._events.error;

      cleanup();

      /* Cannot use `listenerCount` in node <= 0.12. */
      if (!handlers || !handlers.length || handlers === onerror) {
        throw err; /* Unhandled stream error in pipe. */
      }
    }
  }
}

/**
 * Check if `node` is a Unist node.
 *
 * @param {*} node - Value.
 * @return {boolean} - Whether `node` is a Unist node.
 */
function isNode(node) {
  return node && string(node.type) && node.type.length !== 0;
}

/**
 * Check if `fn` is a function.
 *
 * @param {*} fn - Value.
 * @return {boolean} - Whether `fn` is a function.
 */
function isFunction(fn) {
  return typeof fn === 'function';
}

/**
 * Check if `compiler` is a Compiler.
 *
 * @param {*} compiler - Value.
 * @return {boolean} - Whether `compiler` is a Compiler.
 */
function isCompiler(compiler) {
  return isFunction(compiler) && compiler.prototype && isFunction(compiler.prototype.compile);
}

/**
 * Check if `parser` is a Parser.
 *
 * @param {*} parser - Value.
 * @return {boolean} - Whether `parser` is a Parser.
 */
function isParser(parser) {
  return isFunction(parser) && parser.prototype && isFunction(parser.prototype.parse);
}
