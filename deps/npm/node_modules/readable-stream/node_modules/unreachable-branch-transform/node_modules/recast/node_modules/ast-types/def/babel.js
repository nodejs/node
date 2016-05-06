require("./es7");

var types = require("../lib/types");
var defaults = require("../lib/shared").defaults;
var def = types.Type.def;
var or = types.Type.or;

def("Noop")
  .bases("Node")
  .build();

def("DoExpression")
  .bases("Expression")
  .build("body")
  .field("body", [def("Statement")]);

def("Super")
  .bases("Expression")
  .build();

def("BindExpression")
  .bases("Expression")
  .build("object", "callee")
  .field("object", or(def("Expression"), null))
  .field("callee", def("Expression"));

def("Decorator")
  .bases("Node")
  .build("expression")
  .field("expression", def("Expression"));

def("Property")
  .field("decorators",
         or([def("Decorator")], null),
         defaults["null"]);

def("MethodDefinition")
  .field("decorators",
         or([def("Decorator")], null),
         defaults["null"]);

def("MetaProperty")
  .bases("Expression")
  .build("meta", "property")
  .field("meta", def("Identifier"))
  .field("property", def("Identifier"));

def("ParenthesizedExpression")
  .bases("Expression")
  .build("expression")
  .field("expression", def("Expression"));

def("ImportSpecifier")
  .bases("ModuleSpecifier")
  .build("imported", "local")
  .field("imported", def("Identifier"));

def("ImportDefaultSpecifier")
  .bases("ModuleSpecifier")
  .build("local");

def("ImportNamespaceSpecifier")
  .bases("ModuleSpecifier")
  .build("local");

def("ExportDefaultDeclaration")
  .bases("Declaration")
  .build("declaration")
  .field("declaration", or(def("Declaration"), def("Expression")));

def("ExportNamedDeclaration")
  .bases("Declaration")
  .build("declaration", "specifiers", "source")
  .field("declaration", or(def("Declaration"), null))
  .field("specifiers", [def("ExportSpecifier")], defaults.emptyArray)
  .field("source", or(def("Literal"), null), defaults["null"]);

def("ExportSpecifier")
  .bases("ModuleSpecifier")
  .build("local", "exported")
  .field("exported", def("Identifier"));

def("ExportNamespaceSpecifier")
  .bases("Specifier")
  .build("exported")
  .field("exported", def("Identifier"));

def("ExportDefaultSpecifier")
  .bases("Specifier")
  .build("exported")
  .field("exported", def("Identifier"));

def("ExportAllDeclaration")
  .bases("Declaration")
  .build("exported", "source")
  .field("exported", or(def("Identifier"), null))
  .field("source", def("Literal"));

def("CommentBlock")
    .bases("Comment")
    .build("value", /*optional:*/ "leading", "trailing");

def("CommentLine")
    .bases("Comment")
    .build("value", /*optional:*/ "leading", "trailing");
