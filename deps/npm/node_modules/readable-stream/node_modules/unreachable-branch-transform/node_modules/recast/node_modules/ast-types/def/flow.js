require("./es7");

var types = require("../lib/types");
var def = types.Type.def;
var or = types.Type.or;
var defaults = require("../lib/shared").defaults;

// Type Annotations
def("Type").bases("Node");

def("AnyTypeAnnotation")
  .bases("Type")
  .build();

def("MixedTypeAnnotation")
  .bases("Type")
  .build();

def("VoidTypeAnnotation")
  .bases("Type")
  .build();

def("NumberTypeAnnotation")
  .bases("Type")
  .build();

def("NumberLiteralTypeAnnotation")
  .bases("Type")
  .build("value", "raw")
  .field("value", Number)
  .field("raw", String);

def("StringTypeAnnotation")
  .bases("Type")
  .build();

def("StringLiteralTypeAnnotation")
  .bases("Type")
  .build("value", "raw")
  .field("value", String)
  .field("raw", String);

def("BooleanTypeAnnotation")
  .bases("Type")
  .build();

def("BooleanLiteralTypeAnnotation")
  .bases("Type")
  .build("value", "raw")
  .field("value", Boolean)
  .field("raw", String);

def("TypeAnnotation")
  .bases("Node")
  .build("typeAnnotation")
  .field("typeAnnotation", def("Type"));

def("NullableTypeAnnotation")
  .bases("Type")
  .build("typeAnnotation")
  .field("typeAnnotation", def("Type"));

def("NullLiteralTypeAnnotation")
  .bases("Type")
  .build();

def("ThisTypeAnnotation")
  .bases("Type")
  .build();

def("FunctionTypeAnnotation")
  .bases("Type")
  .build("params", "returnType", "rest", "typeParameters")
  .field("params", [def("FunctionTypeParam")])
  .field("returnType", def("Type"))
  .field("rest", or(def("FunctionTypeParam"), null))
  .field("typeParameters", or(def("TypeParameterDeclaration"), null));

def("FunctionTypeParam")
  .bases("Node")
  .build("name", "typeAnnotation", "optional")
  .field("name", def("Identifier"))
  .field("typeAnnotation", def("Type"))
  .field("optional", Boolean);

def("ArrayTypeAnnotation")
  .bases("Type")
  .build("elementType")
  .field("elementType", def("Type"));

def("ObjectTypeAnnotation")
  .bases("Type")
  .build("properties")
  .field("properties", [def("ObjectTypeProperty")])
  .field("indexers", [def("ObjectTypeIndexer")], defaults.emptyArray)
  .field("callProperties",
         [def("ObjectTypeCallProperty")],
         defaults.emptyArray);

def("ObjectTypeProperty")
  .bases("Node")
  .build("key", "value", "optional")
  .field("key", or(def("Literal"), def("Identifier")))
  .field("value", def("Type"))
  .field("optional", Boolean);

def("ObjectTypeIndexer")
  .bases("Node")
  .build("id", "key", "value")
  .field("id", def("Identifier"))
  .field("key", def("Type"))
  .field("value", def("Type"));

def("ObjectTypeCallProperty")
  .bases("Node")
  .build("value")
  .field("value", def("FunctionTypeAnnotation"))
  .field("static", Boolean, defaults["false"]);

def("QualifiedTypeIdentifier")
  .bases("Node")
  .build("qualification", "id")
  .field("qualification",
         or(def("Identifier"),
            def("QualifiedTypeIdentifier")))
  .field("id", def("Identifier"));

def("GenericTypeAnnotation")
  .bases("Type")
  .build("id", "typeParameters")
  .field("id", or(def("Identifier"), def("QualifiedTypeIdentifier")))
  .field("typeParameters", or(def("TypeParameterInstantiation"), null));

def("MemberTypeAnnotation")
  .bases("Type")
  .build("object", "property")
  .field("object", def("Identifier"))
  .field("property",
         or(def("MemberTypeAnnotation"),
            def("GenericTypeAnnotation")));

def("UnionTypeAnnotation")
  .bases("Type")
  .build("types")
  .field("types", [def("Type")]);

def("IntersectionTypeAnnotation")
  .bases("Type")
  .build("types")
  .field("types", [def("Type")]);

def("TypeofTypeAnnotation")
  .bases("Type")
  .build("argument")
  .field("argument", def("Type"));

def("Identifier")
  .field("typeAnnotation", or(def("TypeAnnotation"), null), defaults["null"]);

def("TypeParameterDeclaration")
  .bases("Node")
  .build("params")
  .field("params", [def("Identifier")]);

def("TypeParameterInstantiation")
  .bases("Node")
  .build("params")
  .field("params", [def("Type")]);

def("Function")
  .field("returnType",
         or(def("TypeAnnotation"), null),
         defaults["null"])
  .field("typeParameters",
         or(def("TypeParameterDeclaration"), null),
         defaults["null"]);

def("ClassProperty")
  .build("key", "value", "typeAnnotation", "static")
  .field("value", or(def("Expression"), null))
  .field("typeAnnotation", or(def("TypeAnnotation"), null))
  .field("static", Boolean, defaults["false"]);

def("ClassImplements")
  .field("typeParameters",
         or(def("TypeParameterInstantiation"), null),
         defaults["null"]);

def("InterfaceDeclaration")
  .bases("Declaration")
  .build("id", "body", "extends")
  .field("id", def("Identifier"))
  .field("typeParameters",
         or(def("TypeParameterDeclaration"), null),
         defaults["null"])
  .field("body", def("ObjectTypeAnnotation"))
  .field("extends", [def("InterfaceExtends")]);

def("DeclareInterface")
  .bases("InterfaceDeclaration")
  .build("id", "body", "extends");

def("InterfaceExtends")
  .bases("Node")
  .build("id")
  .field("id", def("Identifier"))
  .field("typeParameters", or(def("TypeParameterInstantiation"), null));

def("TypeAlias")
  .bases("Declaration")
  .build("id", "typeParameters", "right")
  .field("id", def("Identifier"))
  .field("typeParameters", or(def("TypeParameterDeclaration"), null))
  .field("right", def("Type"));

def("DeclareTypeAlias")
  .bases("TypeAlias")
  .build("id", "typeParameters", "right");

def("TypeCastExpression")
  .bases("Expression")
  .build("expression", "typeAnnotation")
  .field("expression", def("Expression"))
  .field("typeAnnotation", def("TypeAnnotation"));

def("TupleTypeAnnotation")
  .bases("Type")
  .build("types")
  .field("types", [def("Type")]);

def("DeclareVariable")
  .bases("Statement")
  .build("id")
  .field("id", def("Identifier"));

def("DeclareFunction")
  .bases("Statement")
  .build("id")
  .field("id", def("Identifier"));

def("DeclareClass")
  .bases("InterfaceDeclaration")
  .build("id");

def("DeclareModule")
  .bases("Statement")
  .build("id", "body")
  .field("id", or(def("Identifier"), def("Literal")))
  .field("body", def("BlockStatement"));

def("DeclareExportDeclaration")
    .bases("Declaration")
    .build("default", "declaration", "specifiers", "source")
    .field("default", Boolean)
    .field("declaration", or(
        def("DeclareVariable"),
        def("DeclareFunction"),
        def("DeclareClass"),
        def("Type"), // Implies default.
        null
    ))
    .field("specifiers", [or(
        def("ExportSpecifier"),
        def("ExportBatchSpecifier")
    )], defaults.emptyArray)
    .field("source", or(
        def("Literal"),
        null
    ), defaults["null"]);
