function* it(children) {
  if (Array.isArray(children)) yield* children;else yield children;
}

function traverse(node, visitorKeys, visitor) {
  const {
    type
  } = node;
  if (!type) return;
  const keys = visitorKeys[type];
  if (!keys) return;

  for (const key of keys) {
    for (const child of it(node[key])) {
      if (child && typeof child === "object") {
        visitor.enter(child);
        traverse(child, visitorKeys, visitor);
        visitor.exit(child);
      }
    }
  }
}

const convertNodesVisitor = {
  enter(node) {
    if (node.innerComments) {
      delete node.innerComments;
    }

    if (node.trailingComments) {
      delete node.trailingComments;
    }

    if (node.leadingComments) {
      delete node.leadingComments;
    }
  },

  exit(node) {
    if (node.extra) {
      delete node.extra;
    }

    if (node != null && node.loc.identifierName) {
      delete node.loc.identifierName;
    }

    if (node.type === "TypeParameter") {
      node.type = "Identifier";
      node.typeAnnotation = node.bound;
      delete node.bound;
    }

    if (node.type === "QualifiedTypeIdentifier") {
      delete node.id;
    }

    if (node.type === "ObjectTypeProperty") {
      delete node.key;
    }

    if (node.type === "ObjectTypeIndexer") {
      delete node.id;
    }

    if (node.type === "FunctionTypeParam") {
      delete node.name;
    }

    if (node.type === "ImportDeclaration") {
      delete node.isType;
    }

    if (node.type === "TemplateLiteral") {
      for (let i = 0; i < node.quasis.length; i++) {
        const q = node.quasis[i];
        q.range[0] -= 1;

        if (q.tail) {
          q.range[1] += 1;
        } else {
          q.range[1] += 2;
        }

        q.loc.start.column -= 1;

        if (q.tail) {
          q.loc.end.column += 1;
        } else {
          q.loc.end.column += 2;
        }
      }
    }
  }

};

function convertNodes(ast, visitorKeys) {
  traverse(ast, visitorKeys, convertNodesVisitor);
}

function convertProgramNode(ast) {
  ast.type = "Program";
  ast.sourceType = ast.program.sourceType;
  ast.body = ast.program.body;
  delete ast.program;
  delete ast.errors;

  if (ast.comments.length) {
    const lastComment = ast.comments[ast.comments.length - 1];

    if (ast.tokens.length) {
      const lastToken = ast.tokens[ast.tokens.length - 1];

      if (lastComment.end > lastToken.end) {
        ast.range[1] = lastToken.end;
        ast.loc.end.line = lastToken.loc.end.line;
        ast.loc.end.column = lastToken.loc.end.column;
      }
    }
  } else {
    if (!ast.tokens.length) {
      ast.loc.start.line = 1;
      ast.loc.end.line = 1;
    }
  }

  if (ast.body && ast.body.length > 0) {
    ast.loc.start.line = ast.body[0].loc.start.line;
    ast.range[0] = ast.body[0].start;
  }
}

module.exports = function convertAST(ast, visitorKeys) {
  convertNodes(ast, visitorKeys);
  convertProgramNode(ast);
};