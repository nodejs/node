(function (global) {
	var lang = {
		INVALID_TYPE: "Type invalide: {type} ({expected} attendu)",
		ENUM_MISMATCH: "Aucune valeur correspondante (enum) pour: {value}",
		ANY_OF_MISSING: "La donnée ne correspond à aucun schema de \"anyOf\"",
		ONE_OF_MISSING: "La donnée ne correspond à aucun schema de  \"oneOf\"",
		ONE_OF_MULTIPLE: "La donnée est valide pour plus d'un schema de \"oneOf\": indices {index1} et {index2}",
		NOT_PASSED: "La donnée correspond au schema de \"not\"",
		// Numeric errors
		NUMBER_MULTIPLE_OF: "La valeur {value} n'est pas un multiple de {multipleOf}",
		NUMBER_MINIMUM: "La valeur {value} est inférieure au minimum {minimum}",
		NUMBER_MINIMUM_EXCLUSIVE: "La valeur {value} est égale au minimum exclusif {minimum}",
		NUMBER_MAXIMUM: "La valeur {value} est supérieure au maximum {maximum}",
		NUMBER_MAXIMUM_EXCLUSIVE: "La valeur {value} est égale au maximum exclusif {maximum}",
		NUMBER_NOT_A_NUMBER: "La valeur {value} n'est pas un nombre valide",
		// String errors
		STRING_LENGTH_SHORT: "Le texte est trop court ({length} carac.), minimum {minimum}",
		STRING_LENGTH_LONG: "Le texte est trop long ({length} carac.), maximum {maximum}",
		STRING_PATTERN: "Le texte ne correspond pas au motif: {pattern}",
		// Object errors
		OBJECT_PROPERTIES_MINIMUM: "Pas assez de propriétés définies ({propertyCount}), minimum {minimum}",
		OBJECT_PROPERTIES_MAXIMUM: "Trop de propriétés définies ({propertyCount}), maximum {maximum}",
		OBJECT_REQUIRED: "Propriété requise manquante: {key}",
		OBJECT_ADDITIONAL_PROPERTIES: "Propriétés additionnelles non autorisées",
		OBJECT_DEPENDENCY_KEY: "Echec de dépendance - la clé doit exister: {missing} (du à la clé: {key})",
		// Array errors
		ARRAY_LENGTH_SHORT: "Le tableau est trop court ({length}), minimum {minimum}",
		ARRAY_LENGTH_LONG: "Le tableau est trop long ({length}), maximum {maximum}",
		ARRAY_UNIQUE: "Des éléments du tableau ne sont pas uniques (indices {match1} et {match2})",
		ARRAY_ADDITIONAL_ITEMS: "Éléments additionnels non autorisés",
		// Format errors
		FORMAT_CUSTOM: "Échec de validation du format ({message})",
		KEYWORD_CUSTOM: "Échec de mot-clé: {key} ({message})",
		// Schema structure
		CIRCULAR_REFERENCE: "Références ($refs) circulaires: {urls}",
		// Non-standard validation options
		UNKNOWN_PROPERTY: "Propriété inconnue (n'existe pas dans le schema)"
	};

	if (typeof define === 'function' && define.amd) {
		// AMD. Register as an anonymous module.
		define(['../tv4'], function(tv4) {
			tv4.addLanguage('fr', lang);
			return tv4;
		});
	} else if (typeof module !== 'undefined' && module.exports){
		// CommonJS. Define export.
		var tv4 = require('../tv4');
		tv4.addLanguage('fr', lang);
		module.exports = tv4;
	} else {
		// Browser globals
		global.tv4.addLanguage('fr', lang);
	}
})(this);
