"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ExportAllDeclaration = ExportAllDeclaration;
exports.ExportDefaultDeclaration = ExportDefaultDeclaration;
exports.ExportDefaultSpecifier = ExportDefaultSpecifier;
exports.ExportNamedDeclaration = ExportNamedDeclaration;
exports.ExportNamespaceSpecifier = ExportNamespaceSpecifier;
exports.ExportSpecifier = ExportSpecifier;
exports.ImportAttribute = ImportAttribute;
exports.ImportDeclaration = ImportDeclaration;
exports.ImportDefaultSpecifier = ImportDefaultSpecifier;
exports.ImportNamespaceSpecifier = ImportNamespaceSpecifier;
exports.ImportSpecifier = ImportSpecifier;

var _t = require("@babel/types");

const {
  isClassDeclaration,
  isExportDefaultSpecifier,
  isExportNamespaceSpecifier,
  isImportDefaultSpecifier,
  isImportNamespaceSpecifier,
  isStatement
} = _t;

function ImportSpecifier(node) {
  if (node.importKind === "type" || node.importKind === "typeof") {
    this.word(node.importKind);
    this.space();
  }

  this.print(node.imported, node);

  if (node.local && node.local.name !== node.imported.name) {
    this.space();
    this.word("as");
    this.space();
    this.print(node.local, node);
  }
}

function ImportDefaultSpecifier(node) {
  this.print(node.local, node);
}

function ExportDefaultSpecifier(node) {
  this.print(node.exported, node);
}

function ExportSpecifier(node) {
  if (node.exportKind === "type") {
    this.word("type");
    this.space();
  }

  this.print(node.local, node);

  if (node.exported && node.local.name !== node.exported.name) {
    this.space();
    this.word("as");
    this.space();
    this.print(node.exported, node);
  }
}

function ExportNamespaceSpecifier(node) {
  this.tokenChar(42);
  this.space();
  this.word("as");
  this.space();
  this.print(node.exported, node);
}

function ExportAllDeclaration(node) {
  this.word("export");
  this.space();

  if (node.exportKind === "type") {
    this.word("type");
    this.space();
  }

  this.tokenChar(42);
  this.space();
  this.word("from");
  this.space();
  this.print(node.source, node);
  this.printAssertions(node);
  this.semicolon();
}

function ExportNamedDeclaration(node) {
  {
    if (this.format.decoratorsBeforeExport && isClassDeclaration(node.declaration)) {
      this.printJoin(node.declaration.decorators, node);
    }
  }
  this.word("export");
  this.space();

  if (node.declaration) {
    const declar = node.declaration;
    this.print(declar, node);
    if (!isStatement(declar)) this.semicolon();
  } else {
    if (node.exportKind === "type") {
      this.word("type");
      this.space();
    }

    const specifiers = node.specifiers.slice(0);
    let hasSpecial = false;

    for (;;) {
      const first = specifiers[0];

      if (isExportDefaultSpecifier(first) || isExportNamespaceSpecifier(first)) {
        hasSpecial = true;
        this.print(specifiers.shift(), node);

        if (specifiers.length) {
          this.tokenChar(44);
          this.space();
        }
      } else {
        break;
      }
    }

    if (specifiers.length || !specifiers.length && !hasSpecial) {
      this.tokenChar(123);

      if (specifiers.length) {
        this.space();
        this.printList(specifiers, node);
        this.space();
      }

      this.tokenChar(125);
    }

    if (node.source) {
      this.space();
      this.word("from");
      this.space();
      this.print(node.source, node);
      this.printAssertions(node);
    }

    this.semicolon();
  }
}

function ExportDefaultDeclaration(node) {
  {
    if (this.format.decoratorsBeforeExport && isClassDeclaration(node.declaration)) {
      this.printJoin(node.declaration.decorators, node);
    }
  }
  this.word("export");
  this.space();
  this.word("default");
  this.space();
  const declar = node.declaration;
  this.print(declar, node);
  if (!isStatement(declar)) this.semicolon();
}

function ImportDeclaration(node) {
  this.word("import");
  this.space();
  const isTypeKind = node.importKind === "type" || node.importKind === "typeof";

  if (isTypeKind) {
    this.word(node.importKind);
    this.space();
  }

  const specifiers = node.specifiers.slice(0);
  const hasSpecifiers = !!specifiers.length;

  while (hasSpecifiers) {
    const first = specifiers[0];

    if (isImportDefaultSpecifier(first) || isImportNamespaceSpecifier(first)) {
      this.print(specifiers.shift(), node);

      if (specifiers.length) {
        this.tokenChar(44);
        this.space();
      }
    } else {
      break;
    }
  }

  if (specifiers.length) {
    this.tokenChar(123);
    this.space();
    this.printList(specifiers, node);
    this.space();
    this.tokenChar(125);
  } else if (isTypeKind && !hasSpecifiers) {
    this.tokenChar(123);
    this.tokenChar(125);
  }

  if (hasSpecifiers || isTypeKind) {
    this.space();
    this.word("from");
    this.space();
  }

  this.print(node.source, node);
  this.printAssertions(node);
  {
    var _node$attributes;

    if ((_node$attributes = node.attributes) != null && _node$attributes.length) {
      this.space();
      this.word("with");
      this.space();
      this.printList(node.attributes, node);
    }
  }
  this.semicolon();
}

function ImportAttribute(node) {
  this.print(node.key);
  this.tokenChar(58);
  this.space();
  this.print(node.value);
}

function ImportNamespaceSpecifier(node) {
  this.tokenChar(42);
  this.space();
  this.word("as");
  this.space();
  this.print(node.local, node);
}