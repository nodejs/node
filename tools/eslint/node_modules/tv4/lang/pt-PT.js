(function (global) {
	var lang = {
		INVALID_TYPE: "Tipo inválido: {type} (esperava {expected})",
		ENUM_MISMATCH: "Nenhuma correspondência 'enum' para: {value}",
		ANY_OF_MISSING: "Os dados não correspondem a nenhum esquema de \"anyOf\"",
		ONE_OF_MISSING: "Os dados não correspondem a nenhum esquema de \"oneOf\"",
		ONE_OF_MULTIPLE: "Os dados são válidos quando comparados com mais de um esquema de \"oneOf\": índices {index1} e {index2}",
		NOT_PASSED: "Os dados correspondem a um esquema de \"not\"",
		// Numeric errors
		NUMBER_MULTIPLE_OF: "O valor {value} não é um múltiplo de {multipleOf}",
		NUMBER_MINIMUM: "O valor {value} é menor que o mínimo {minimum}",
		NUMBER_MINIMUM_EXCLUSIVE: "O valor {value} é igual ao mínimo exclusivo {minimum}",
		NUMBER_MAXIMUM: "O valor {value} é maior que o máximo {maximum}",
		NUMBER_MAXIMUM_EXCLUSIVE: "O valor {value} é igual ao máximo exclusivo {maximum}",
		NUMBER_NOT_A_NUMBER: "O valor {value} não é um número válido",
		// String errors
		STRING_LENGTH_SHORT: "A 'string' é muito curta ({length} caracteres), mínimo {minimum}",
		STRING_LENGTH_LONG: "A 'string' é muito longa ({length} caracteres), máximo {maximum}",
		STRING_PATTERN: "A 'string' não corresponde ao modelo: {pattern}",
		// Object errors
		OBJECT_PROPERTIES_MINIMUM: "Poucas propriedades definidas ({propertyCount}), mínimo {minimum}",
		OBJECT_PROPERTIES_MAXIMUM: "Muitas propriedades definidas ({propertyCount}), máximo {maximum}",
		OBJECT_REQUIRED: "Propriedade necessária em falta: {key}",
		OBJECT_ADDITIONAL_PROPERTIES: "Não são permitidas propriedades adicionais",
		OBJECT_DEPENDENCY_KEY: "Uma dependência falhou - tem de existir uma chave: {missing} (devido à chave: {key})",
		// Array errors
		ARRAY_LENGTH_SHORT: "A 'array' é muito curta ({length}), mínimo {minimum}",
		ARRAY_LENGTH_LONG: "A 'array' é muito longa ({length}), máximo {maximum}",
		ARRAY_UNIQUE: "Os itens da 'array' não são únicos (índices {match1} e {match2})",
		ARRAY_ADDITIONAL_ITEMS: "Não são permitidos itens adicionais",
		// Format errors
		FORMAT_CUSTOM: "A validação do formato falhou ({message})",
		KEYWORD_CUSTOM: "A 'keyword' falhou: {key} ({message})",
		// Schema structure
		CIRCULAR_REFERENCE: "$refs circular: {urls}",
		// Non-standard validation options
		UNKNOWN_PROPERTY: "Propriedade desconhecida (não está em 'schema')"
	};
	
	if (typeof define === 'function' && define.amd) {
		// AMD. Register as an anonymous module.
		define(['../tv4'], function(tv4) {
			tv4.addLanguage('pt-PT', lang);
			return tv4;
		});
	} else if (typeof module !== 'undefined' && module.exports) {
		// CommonJS. Define export.
		var tv4 = require('../tv4');
		tv4.addLanguage('pt-PT', lang);
		module.exports = tv4;
	} else {
		// Browser globals
		global.tv4.addLanguage('pt-PT', lang);
	}
})(this);
