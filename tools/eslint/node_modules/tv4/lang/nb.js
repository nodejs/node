(function (global) {
	var lang = {
		INVALID_TYPE: "Ugyldig type: {type} (forventet {expected})",
		ENUM_MISMATCH: "Ingen samsvarende enum verdi for: {value}",
		ANY_OF_MISSING: "Data samsvarer ikke med noe skjema fra \"anyOf\"",
		ONE_OF_MISSING: "Data samsvarer ikke med noe skjema fra \"oneOf\"",
		ONE_OF_MULTIPLE: "Data samsvarer med mer enn ett skjema fra \"oneOf\": indeks {index1} og {index2}",
		NOT_PASSED: "Data samsvarer med skjema fra \"not\"",
		// Numeric errors
		NUMBER_MULTIPLE_OF: "Verdien {value} er ikke et multiplum av {multipleOf}",
		NUMBER_MINIMUM: "Verdien {value} er mindre enn minsteverdi {minimum}",
		NUMBER_MINIMUM_EXCLUSIVE: "Verdien {value} er lik eksklusiv minsteverdi {minimum}",
		NUMBER_MAXIMUM: "Verdien {value} er større enn maksimalverdi {maximum}",
		NUMBER_MAXIMUM_EXCLUSIVE: "Verdien {value} er lik eksklusiv maksimalverdi {maximum}",
		NUMBER_NOT_A_NUMBER: "Verdien {value} er ikke et gyldig tall",
		// String errors
		STRING_LENGTH_SHORT: "Strengen er for kort ({length} tegn), minst {minimum}",
		STRING_LENGTH_LONG: "Strengen er for lang ({length} tegn), maksimalt {maximum}",
		STRING_PATTERN: "Strengen samsvarer ikke med regulært uttrykk: {pattern}",
		// Object errors
		OBJECT_PROPERTIES_MINIMUM: "For få variabler definert ({propertyCount}), minst {minimum} er forventet",
		OBJECT_PROPERTIES_MAXIMUM: "For mange variabler definert ({propertyCount}), makismalt {maximum} er tillatt",
		OBJECT_REQUIRED: "Mangler obligatorisk variabel: {key}",
		OBJECT_ADDITIONAL_PROPERTIES: "Tilleggsvariabler er ikke tillatt",
		OBJECT_DEPENDENCY_KEY: "Variabelen {missing} må være definert (på grunn av følgende variabel: {key})",
		// Array errors
		ARRAY_LENGTH_SHORT: "Listen er for kort ({length} elementer), minst {minimum}",
		ARRAY_LENGTH_LONG: "Listen er for lang ({length} elementer), maksimalt {maximum}",
		ARRAY_UNIQUE: "Elementene er ikke unike (indeks {match1} og {match2} er like)",
		ARRAY_ADDITIONAL_ITEMS: "Tillegselementer er ikke tillatt",
		// Format errors
		FORMAT_CUSTOM: "Formatteringen stemmer ikke ({message})",
		KEYWORD_CUSTOM: "Nøkkelen stemmer ikke: {key} ({message})",
		// Schema structure
		CIRCULAR_REFERENCE: "Sirkulære referanser ($refs): {urls}",
		// Non-standard validation options
		UNKNOWN_PROPERTY: "Ukjent variabel (eksisterer ikke i skjemaet)"
	};
	
	if (typeof define === 'function' && define.amd) {
		// AMD. Register as an anonymous module.
		define(['../tv4'], function(tv4) {
			tv4.addLanguage('nb', lang);
			return tv4;
		});
	} else if (typeof module !== 'undefined' && module.exports) {
		// CommonJS. Define export.
		var tv4 = require('../tv4');
		tv4.addLanguage('nb', lang);
		module.exports = tv4;
	} else {
		// Browser globals
		global.tv4.addLanguage('nb', lang);
	}
})(this);

