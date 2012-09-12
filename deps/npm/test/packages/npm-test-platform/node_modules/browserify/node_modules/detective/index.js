var uglify = require('uglify-js');

var traverse = function (node, cb, parent, grandparent) {
    // Call cb on all good AST nodes.
    if (Array.isArray(node) && node[0]
    && typeof node[0] === 'object' && node[0].name) {
        cb({ name : node[0].name, value : node.slice(1) , grandparent: grandparent});
    }
    
    // Traverse down the tree on arrays and objects.
    if (Array.isArray(node)
    || Object.prototype.toString.call(node) === "[object Object]") {
        for (var key in node) traverse(node[key], cb, node, parent);
    }
};

var walk = function (src, cb) {
    var ast = uglify.parser.parse(src.toString(), false, true);
    traverse(ast, cb);    
};

var deparse = function (ast) {
    return uglify.uglify.gen_code(ast);
};

var exports = module.exports = function (src, opts) {
    return exports.find(src, opts).strings;
};

exports.find = function (src, opts) {
    if (!opts) opts = {};
    var word = opts.word === undefined ? 'require' : opts.word;
    
    var modules = { strings : [], expressions : [] };
    
    if (src.toString().indexOf(word) == -1) return modules;
    
    walk(src, function (node) {
        var gp = node.grandparent;
        var isRequire = Array.isArray(gp)
            && gp[0] 
            && (gp[0] === 'call' || gp[0].name === 'call')
            && gp[1][0] === 'name'
            && gp[1][1] === word
        ;
        if(isRequire) {
            if(node.name === 'string') {
                modules.strings.push(node.value[0]);
            } else {
                modules.expressions.push(deparse(gp[2][0]));
            }
        }
    });
    
    return modules;
};
