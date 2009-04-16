// module search paths
node.includes = ["."];

node.path = new function () {
    this.join = function () {
        var joined = "";
        for (var i = 0; i < arguments.length; i++) {
            var part = arguments[i].toString();
            if (i === 0) {
                part = part.replace(/\/*$/, "/");
            } else if (i === arguments.length - 1) {
                part = part.replace(/^\/*/, "");
            } else {
                part = part.replace(/^\/*/, "")
                           .replace(/\/*$/, "/");
            }
            joined += part;
        }
        return joined;
    };

    this.dirname = function (path) {
        var parts = path.split("/");
        return parts.slice(0, parts.length-1);
    };
};

function __include (module, path) {
    var export = module.require(path);
    for (var i in export) {
        if (export.hasOwnProperty(i))
            module[i] = export[i];
    }
}

function __require (path, loading_file) {

    var filename = path;
    // relative path
    // absolute path
    if (path.slice(0,1) === "/") {
    } else {
        //filename = node.path.join(node.path.dirname(loading_file), path);
    }
    stdout.puts("require: " + filename);
    
    var source = node.blocking.cat(filename);

    // wrap the source in a function 
    source = "function (__file__, __dir__) { " 
           + "  var exports = {};"
           + "  function require (m) { return __require(m, __file__); }"
           + "  function include (m) { return __include(this, m); }"
           +    source 
           + "  return exports;"
           + "};"
           ;
    var create_module = node.blocking.exec(source, filename);

    // execute the function wrap
    return create_module(filename, node.path.dirname(filename));
}

// main script execution.
__require(ARGV[1], ".");

