require("./core");
var types = require("../lib/types");
var def = types.Type.def;
var or = types.Type.or;
var defaults = require("../lib/shared").defaults;

def("Function")
    .field("generator", Boolean, defaults["false"])
    .field("expression", Boolean, defaults["false"])
    .field("defaults", [or(def("Expression"), null)], defaults.emptyArray)
    // TODO This could be represented as a RestElement in .params.
    .field("rest", or(def("Identifier"), null), defaults["null"]);

// The ESTree way of representing a ...rest parameter.
def("RestElement")
    .bases("Pattern")
    .build("argument")
    .field("argument", def("Pattern"));

def("SpreadElementPattern")
    .bases("Pattern")
    .build("argument")
    .field("argument", def("Pattern"));

def("FunctionDeclaration")
    .build("id", "params", "body", "generator", "expression");

def("FunctionExpression")
    .build("id", "params", "body", "generator", "expression");

// The Parser API calls this ArrowExpression, but Esprima and all other
// actual parsers use ArrowFunctionExpression.
def("ArrowFunctionExpression")
    .bases("Function", "Expression")
    .build("params", "body", "expression")
    // The forced null value here is compatible with the overridden
    // definition of the "id" field in the Function interface.
    .field("id", null, defaults["null"])
    // Arrow function bodies are allowed to be expressions.
    .field("body", or(def("BlockStatement"), def("Expression")))
    // The current spec forbids arrow generators, so I have taken the
    // liberty of enforcing that. TODO Report this.
    .field("generator", false, defaults["false"]);

def("YieldExpression")
    .bases("Expression")
    .build("argument", "delegate")
    .field("argument", or(def("Expression"), null))
    .field("delegate", Boolean, defaults["false"]);

def("GeneratorExpression")
    .bases("Expression")
    .build("body", "blocks", "filter")
    .field("body", def("Expression"))
    .field("blocks", [def("ComprehensionBlock")])
    .field("filter", or(def("Expression"), null));

def("ComprehensionExpression")
    .bases("Expression")
    .build("body", "blocks", "filter")
    .field("body", def("Expression"))
    .field("blocks", [def("ComprehensionBlock")])
    .field("filter", or(def("Expression"), null));

def("ComprehensionBlock")
    .bases("Node")
    .build("left", "right", "each")
    .field("left", def("Pattern"))
    .field("right", def("Expression"))
    .field("each", Boolean);

def("Property")
    .field("key", or(def("Literal"), def("Identifier"), def("Expression")))
    .field("value", or(def("Expression"), def("Pattern")))
    .field("method", Boolean, defaults["false"])
    .field("shorthand", Boolean, defaults["false"])
    .field("computed", Boolean, defaults["false"]);

def("PropertyPattern")
    .bases("Pattern")
    .build("key", "pattern")
    .field("key", or(def("Literal"), def("Identifier"), def("Expression")))
    .field("pattern", def("Pattern"))
    .field("computed", Boolean, defaults["false"]);

def("ObjectPattern")
    .bases("Pattern")
    .build("properties")
    .field("properties", [or(def("PropertyPattern"), def("Property"))]);

def("ArrayPattern")
    .bases("Pattern")
    .build("elements")
    .field("elements", [or(def("Pattern"), null)]);

def("MethodDefinition")
    .bases("Declaration")
    .build("kind", "key", "value", "static")
    .field("kind", or("constructor", "method", "get", "set"))
    .field("key", or(def("Literal"), def("Identifier"), def("Expression")))
    .field("value", def("Function"))
    .field("computed", Boolean, defaults["false"])
    .field("static", Boolean, defaults["false"]);

def("SpreadElement")
    .bases("Node")
    .build("argument")
    .field("argument", def("Expression"));

def("ArrayExpression")
    .field("elements", [or(
        def("Expression"),
        def("SpreadElement"),
        def("RestElement"),
        null
    )]);

def("NewExpression")
    .field("arguments", [or(def("Expression"), def("SpreadElement"))]);

def("CallExpression")
    .field("arguments", [or(def("Expression"), def("SpreadElement"))]);

// Note: this node type is *not* an AssignmentExpression with a Pattern on
// the left-hand side! The existing AssignmentExpression type already
// supports destructuring assignments. AssignmentPattern nodes may appear
// wherever a Pattern is allowed, and the right-hand side represents a
// default value to be destructured against the left-hand side, if no
// value is otherwise provided. For example: default parameter values.
def("AssignmentPattern")
    .bases("Pattern")
    .build("left", "right")
    .field("left", def("Pattern"))
    .field("right", def("Expression"));

var ClassBodyElement = or(
    def("MethodDefinition"),
    def("VariableDeclarator"),
    def("ClassPropertyDefinition"),
    def("ClassProperty")
);

def("ClassProperty")
  .bases("Declaration")
  .build("key")
  .field("key", or(def("Literal"), def("Identifier"), def("Expression")))
  .field("computed", Boolean, defaults["false"]);

def("ClassPropertyDefinition") // static property
    .bases("Declaration")
    .build("definition")
    // Yes, Virginia, circular definitions are permitted.
    .field("definition", ClassBodyElement);

def("ClassBody")
    .bases("Declaration")
    .build("body")
    .field("body", [ClassBodyElement]);

def("ClassDeclaration")
    .bases("Declaration")
    .build("id", "body", "superClass")
    .field("id", or(def("Identifier"), null))
    .field("body", def("ClassBody"))
    .field("superClass", or(def("Expression"), null), defaults["null"]);

def("ClassExpression")
    .bases("Expression")
    .build("id", "body", "superClass")
    .field("id", or(def("Identifier"), null), defaults["null"])
    .field("body", def("ClassBody"))
    .field("superClass", or(def("Expression"), null), defaults["null"])
    .field("implements", [def("ClassImplements")], defaults.emptyArray);

def("ClassImplements")
    .bases("Node")
    .build("id")
    .field("id", def("Identifier"))
    .field("superClass", or(def("Expression"), null), defaults["null"]);

// Specifier and ModuleSpecifier are abstract non-standard types
// introduced for definitional convenience.
def("Specifier").bases("Node");

// This supertype is shared/abused by both def/babel.js and
// def/esprima.js. In the future, it will be possible to load only one set
// of definitions appropriate for a given parser, but until then we must
// rely on default functions to reconcile the conflicting AST formats.
def("ModuleSpecifier")
    .bases("Specifier")
    // This local field is used by Babel/Acorn. It should not technically
    // be optional in the Babel/Acorn AST format, but it must be optional
    // in the Esprima AST format.
    .field("local", or(def("Identifier"), null), defaults["null"])
    // The id and name fields are used by Esprima. The id field should not
    // technically be optional in the Esprima AST format, but it must be
    // optional in the Babel/Acorn AST format.
    .field("id", or(def("Identifier"), null), defaults["null"])
    .field("name", or(def("Identifier"), null), defaults["null"]);

def("TaggedTemplateExpression")
    .bases("Expression")
    .build("tag", "quasi")
    .field("tag", def("Expression"))
    .field("quasi", def("TemplateLiteral"));

def("TemplateLiteral")
    .bases("Expression")
    .build("quasis", "expressions")
    .field("quasis", [def("TemplateElement")])
    .field("expressions", [def("Expression")]);

def("TemplateElement")
    .bases("Node")
    .build("value", "tail")
    .field("value", {"cooked": String, "raw": String})
    .field("tail", Boolean);
