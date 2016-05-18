(function (global) {
	var lang = {
  	INVALID_TYPE: "Otillåten typ: {type} (skall vara {expected})",
  	ENUM_MISMATCH: "Otillåtet värde: {value}",
  	ANY_OF_MISSING: "Värdet matchar inget av schemana \"anyOf\"",
  	ONE_OF_MISSING: "Värdet matchar inget av schemana \"oneOf\"",
  	ONE_OF_MULTIPLE: "Värdet matchar flera scheman \"oneOf\": index {index1} och {index2}",
  	NOT_PASSED: "Värdet matchar schemat från \"not\"",
  	// Numeric errors
  	NUMBER_MULTIPLE_OF: "Värdet {value} är inte en multipel av {multipleOf}",
  	NUMBER_MINIMUM: "Värdet {value} får inte vara mindre än {minimum}",
  	NUMBER_MINIMUM_EXCLUSIVE: "Värdet {value} måste vara större än {minimum}",
  	NUMBER_MAXIMUM: "Värdet {value} får inte vara större än {maximum}",
  	NUMBER_MAXIMUM_EXCLUSIVE: "Värdet {value} måste vara mindre än {maximum}",
  	NUMBER_NOT_A_NUMBER: "Värdet {value} är inte ett giltigt tal",
  	// String errors
  	STRING_LENGTH_SHORT: "Texten är för kort ({length} tecken), ska vara minst {minimum} tecken",
  	STRING_LENGTH_LONG: "Texten är för lång ({length} tecken), ska vara högst {maximum}",
  	STRING_PATTERN: "Texten har fel format: {pattern}",
  	// Object errors
  	OBJECT_PROPERTIES_MINIMUM: "För få parametrar ({propertyCount}), ska minst vara {minimum}",
  	OBJECT_PROPERTIES_MAXIMUM: "För många parametrar ({propertyCount}), får högst vara {maximum}",
  	OBJECT_REQUIRED: "Egenskap saknas: {key}",
  	OBJECT_ADDITIONAL_PROPERTIES: "Extra parametrar är inte tillåtna",
  	OBJECT_DEPENDENCY_KEY: "Saknar beroende - saknad nyckel: {missing} (beroende nyckel: {key})",
  	// Array errors
  	ARRAY_LENGTH_SHORT: "Listan är för kort ({length}), ska minst vara {minimum}",
  	ARRAY_LENGTH_LONG: "Listan är för lång ({length}), ska högst vara {maximum}",
  	ARRAY_UNIQUE: "Listvärden är inte unika (index {match1} och {match2})",
  	ARRAY_ADDITIONAL_ITEMS: "Extra värden är inte tillåtna",
  	// Format errors
  	FORMAT_CUSTOM: "Misslyckad validering ({message})",
  	KEYWORD_CUSTOM: "Misslyckat nyckelord: {key} ({message})",
  	// Schema structure
  	CIRCULAR_REFERENCE: "Cirkulär $refs: {urls}",
  	// Non-standard validation options
  	UNKNOWN_PROPERTY: "Okänd egenskap (finns ej i schema)"
	};
	
	if (typeof define === 'function' && define.amd) {
		// AMD. Register as an anonymous module.
		define(['../tv4'], function(tv4) {
			tv4.addLanguage('sv-SE', lang);
			return tv4;
		});
	} else if (typeof module !== 'undefined' && module.exports) {
		// CommonJS. Define export.
		var tv4 = require('../tv4');
		tv4.addLanguage('sv-SE', lang);
		module.exports = tv4;
	} else {
		// Browser globals
		global.tv4.addLanguage('sv-SE', lang);
	}
})(this);
