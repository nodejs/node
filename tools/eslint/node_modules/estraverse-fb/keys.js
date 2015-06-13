var unprefixedKeys = {
	Identifier: [],
	NamespacedName: ['namespace', 'name'],
	MemberExpression: ['object', 'property'],
	EmptyExpression: [],
	ExpressionContainer: ['expression'],
	Element: ['openingElement', 'closingElement', 'children'],
	ClosingElement: ['name'],
	OpeningElement: ['name', 'attributes'],
	Attribute: ['name', 'value'],
	Text: null,
	SpreadAttribute: ['argument']
};

var flowKeys = {
	Type: [],
	AnyTypeAnnotation: [],
	VoidTypeAnnotation: [],
	NumberTypeAnnotation: [],
	StringTypeAnnotation: [],
	StringLiteralTypeAnnotation: ["value", "raw"],
	BooleanTypeAnnotation: [],
	TypeAnnotation: ["typeAnnotation"],
	NullableTypeAnnotation: ["typeAnnotation"],
	FunctionTypeAnnotation: ["params", "returnType", "rest", "typeParameters"],
	FunctionTypeParam: ["name", "typeAnnotation", "optional"],
	ObjectTypeAnnotation: ["properties"],
	ObjectTypeProperty: ["key", "value", "optional"],
	ObjectTypeIndexer: ["id", "key", "value"],
	ObjectTypeCallProperty: ["value"],
	QualifiedTypeIdentifier: ["qualification", "id"],
	GenericTypeAnnotation: ["id", "typeParameters"],
	MemberTypeAnnotation: ["object", "property"],
	UnionTypeAnnotation: ["types"],
	IntersectionTypeAnnotation: ["types"],
	TypeofTypeAnnotation: ["argument"],
	TypeParameterDeclaration: ["params"],
	TypeParameterInstantiation: ["params"],
	ClassProperty: ["key", "typeAnnotation"],
	ClassImplements: [],
	InterfaceDeclaration: ["id", "body", "extends"],
	InterfaceExtends: ["id"],
	TypeAlias: ["id", "typeParameters", "right"],
	TupleTypeAnnotation: ["types"],
	DeclareVariable: ["id"],
	DeclareFunction: ["id"],
	DeclareClass: ["id"],
	DeclareModule: ["id", "body"]
};

for (var key in unprefixedKeys) {
	exports['XJS' + key] = exports['JSX' + key] = unprefixedKeys[key];
}

for (var key in flowKeys) {
	exports[key] = flowKeys[key];
}