(function (global) {
	var lang = {
        INVALID_TYPE: "Niepoprawny typ: {type} (spodziewany {expected})",
        ENUM_MISMATCH: "Żadna predefiniowana wartośc nie pasuje do: {value}",
        ANY_OF_MISSING: "Dane nie pasują do żadnego wzoru z sekcji \"anyOf\"",
        ONE_OF_MISSING: "Dane nie pasują do żadnego wzoru z sekcji \"oneOf\"",
        ONE_OF_MULTIPLE: "Dane są prawidłowe dla więcej niż jednego schematu z \"oneOf\": indeksy {index1} i {index2}",
        NOT_PASSED: "Dane pasują do wzoru z sekcji \"not\"",
        // Numeric errors
        NUMBER_MULTIPLE_OF: "Wartość {value} nie jest wielokrotnością {multipleOf}",
        NUMBER_MINIMUM: "Wartość {value} jest mniejsza niż {minimum}",
        NUMBER_MINIMUM_EXCLUSIVE: "Wartość {value} jest równa wyłączonemu minimum {minimum}",
        NUMBER_MAXIMUM: "Wartość {value} jest większa niż {maximum}",
        NUMBER_MAXIMUM_EXCLUSIVE: "Wartość {value} jest równa wyłączonemu maksimum {maximum}",
        NUMBER_NOT_A_NUMBER: "Wartość {value} nie jest poprawną liczbą",
        // String errors
        STRING_LENGTH_SHORT: "Napis jest za krótki ({length} znaków), minimum {minimum}",
        STRING_LENGTH_LONG: "Napis jest za długi ({length} )znaków, maksimum {maximum}",
        STRING_PATTERN: "Napis nie pasuje do wzoru: {pattern}",
        // Object errors
        OBJECT_PROPERTIES_MINIMUM: "Za mało zdefiniowanych pól ({propertyCount}), minimum {minimum}",
        OBJECT_PROPERTIES_MAXIMUM: "Za dużo zdefiniowanych pól ({propertyCount}), maksimum {maximum}",
        OBJECT_REQUIRED: "Brakuje wymaganego pola: {key}",
        OBJECT_ADDITIONAL_PROPERTIES: "Dodatkowe pola są niedozwolone",
        OBJECT_DEPENDENCY_KEY: "Błąd zależności - klucz musi istnieć: {missing} (wzgledem klucza: {key})",
        // Array errors
        ARRAY_LENGTH_SHORT: "Tablica ma za mało elementów ({length}), minimum {minimum}",
        ARRAY_LENGTH_LONG: "Tablica ma za dużo elementów ({length}), maximum {maximum}",
        ARRAY_UNIQUE: "Elementy tablicy nie są unikalne (indeks {match1} i {match2})",
        ARRAY_ADDITIONAL_ITEMS: "Dodatkowe elementy są niedozwolone",
        // Format errors
        FORMAT_CUSTOM: "Błąd zgodności z formatem ({message})",
        KEYWORD_CUSTOM: "Błąd słowa kluczowego: {key} ({message})",
        // Schema structure
        CIRCULAR_REFERENCE: "Cykliczna referencja $refs: {urls}",
        // Non-standard validation options
        UNKNOWN_PROPERTY: "Nie znane pole (brak we wzorze(schema))"
	};
	
	if (typeof define === 'function' && define.amd) {
		// AMD. Register as an anonymous module.
		define(['../tv4'], function(tv4) {
			tv4.addLanguage('pl-PL', lang);
			return tv4;
		});
	} else if (typeof module !== 'undefined' && module.exports) {
		// CommonJS. Define export.
		var tv4 = require('../tv4');
		tv4.addLanguage('pl-PL', lang);
		module.exports = tv4;
	} else {
		// Browser globals
		global.tv4.addLanguage('pl-PL', lang);
	}
})(this);
