macro IS_BOOLEAN(arg)           = (typeof(arg) === 'boolean');
macro IS_NULL(arg)              = (arg === null);
macro IS_NULL_OR_UNDEFINED(arg) = (arg == null);
macro IS_NUMBER(arg)            = (typeof(arg) === 'number');
macro IS_STRING(arg)            = (typeof(arg) === 'string');
macro IS_SYMBOL(arg)            = (typeof(arg) === 'symbol');
macro IS_UNDEFINED(arg)         = (typeof(arg) === 'undefined');

# These macros follow the semantics of V8's %_Is*() functions.
macro IS_ARRAY(arg)             = (Array.isArray(arg));
macro IS_DATE(arg)              = ((arg) instanceof Date);
macro IS_FUNCTION(arg)          = (typeof(arg) === 'function');
macro IS_OBJECT(arg)            = (typeof(arg) === 'object');
macro IS_REGEXP(arg)            = ((arg) instanceof RegExp);

macro IS_BUFFER(arg)            = ((arg) instanceof Buffer);
