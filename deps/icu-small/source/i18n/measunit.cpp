// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
* Copyright (c) 2004-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "utypeinfo.h" // for 'typeid' to work

#include "unicode/measunit.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uenum.h"
#include "ustrenum.h"
#include "cstring.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MeasureUnit)

// All code between the "Start generated code" comment and
// the "End generated code" comment is auto generated code
// and must not be edited manually. For instructions on how to correctly
// update this code, refer to:
// http://site.icu-project.org/design/formatting/measureformat/updating-measure-unit
//
// Start generated code


static const int32_t gOffsets[] = {
    0,
    2,
    7,
    16,
    22,
    26,
    325,
    336,
    347,
    351,
    357,
    361,
    381,
    382,
    393,
    396,
    402,
    408,
    412,
    416,
    441
};

static const int32_t gIndexes[] = {
    0,
    2,
    7,
    16,
    22,
    26,
    26,
    37,
    48,
    52,
    58,
    62,
    82,
    83,
    94,
    97,
    103,
    109,
    113,
    117,
    142
};

// Must be sorted alphabetically.
static const char * const gTypes[] = {
    "acceleration",
    "angle",
    "area",
    "concentr",
    "consumption",
    "currency",
    "digital",
    "duration",
    "electric",
    "energy",
    "frequency",
    "length",
    "light",
    "mass",
    "none",
    "power",
    "pressure",
    "speed",
    "temperature",
    "volume"
};

// Must be grouped by type and sorted alphabetically within each type.
static const char * const gSubTypes[] = {
    "g-force",
    "meter-per-second-squared",
    "arc-minute",
    "arc-second",
    "degree",
    "radian",
    "revolution",
    "acre",
    "hectare",
    "square-centimeter",
    "square-foot",
    "square-inch",
    "square-kilometer",
    "square-meter",
    "square-mile",
    "square-yard",
    "karat",
    "milligram-per-deciliter",
    "millimole-per-liter",
    "part-per-million",
    "percent",
    "permille",
    "liter-per-100kilometers",
    "liter-per-kilometer",
    "mile-per-gallon",
    "mile-per-gallon-imperial",
    "ADP",
    "AED",
    "AFA",
    "AFN",
    "ALK",
    "ALL",
    "AMD",
    "ANG",
    "AOA",
    "AOK",
    "AON",
    "AOR",
    "ARA",
    "ARP",
    "ARS",
    "ARY",
    "ATS",
    "AUD",
    "AWG",
    "AYM",
    "AZM",
    "AZN",
    "BAD",
    "BAM",
    "BBD",
    "BDT",
    "BEC",
    "BEF",
    "BEL",
    "BGJ",
    "BGK",
    "BGL",
    "BGN",
    "BHD",
    "BIF",
    "BMD",
    "BND",
    "BOB",
    "BOP",
    "BOV",
    "BRB",
    "BRC",
    "BRE",
    "BRL",
    "BRN",
    "BRR",
    "BSD",
    "BTN",
    "BUK",
    "BWP",
    "BYB",
    "BYN",
    "BYR",
    "BZD",
    "CAD",
    "CDF",
    "CHC",
    "CHE",
    "CHF",
    "CHW",
    "CLF",
    "CLP",
    "CNY",
    "COP",
    "COU",
    "CRC",
    "CSD",
    "CSJ",
    "CSK",
    "CUC",
    "CUP",
    "CVE",
    "CYP",
    "CZK",
    "DDM",
    "DEM",
    "DJF",
    "DKK",
    "DOP",
    "DZD",
    "ECS",
    "ECV",
    "EEK",
    "EGP",
    "ERN",
    "ESA",
    "ESB",
    "ESP",
    "ETB",
    "EUR",
    "FIM",
    "FJD",
    "FKP",
    "FRF",
    "GBP",
    "GEK",
    "GEL",
    "GHC",
    "GHP",
    "GHS",
    "GIP",
    "GMD",
    "GNE",
    "GNF",
    "GNS",
    "GQE",
    "GRD",
    "GTQ",
    "GWE",
    "GWP",
    "GYD",
    "HKD",
    "HNL",
    "HRD",
    "HRK",
    "HTG",
    "HUF",
    "IDR",
    "IEP",
    "ILP",
    "ILR",
    "ILS",
    "INR",
    "IQD",
    "IRR",
    "ISJ",
    "ISK",
    "ITL",
    "JMD",
    "JOD",
    "JPY",
    "KES",
    "KGS",
    "KHR",
    "KMF",
    "KPW",
    "KRW",
    "KWD",
    "KYD",
    "KZT",
    "LAJ",
    "LAK",
    "LBP",
    "LKR",
    "LRD",
    "LSL",
    "LSM",
    "LTL",
    "LTT",
    "LUC",
    "LUF",
    "LUL",
    "LVL",
    "LVR",
    "LYD",
    "MAD",
    "MDL",
    "MGA",
    "MGF",
    "MKD",
    "MLF",
    "MMK",
    "MNT",
    "MOP",
    "MRO",
    "MRU",
    "MTL",
    "MTP",
    "MUR",
    "MVQ",
    "MVR",
    "MWK",
    "MXN",
    "MXP",
    "MXV",
    "MYR",
    "MZE",
    "MZM",
    "MZN",
    "NAD",
    "NGN",
    "NIC",
    "NIO",
    "NLG",
    "NOK",
    "NPR",
    "NZD",
    "OMR",
    "PAB",
    "PEH",
    "PEI",
    "PEN",
    "PES",
    "PGK",
    "PHP",
    "PKR",
    "PLN",
    "PLZ",
    "PTE",
    "PYG",
    "QAR",
    "RHD",
    "ROK",
    "ROL",
    "RON",
    "RSD",
    "RUB",
    "RUR",
    "RWF",
    "SAR",
    "SBD",
    "SCR",
    "SDD",
    "SDG",
    "SDP",
    "SEK",
    "SGD",
    "SHP",
    "SIT",
    "SKK",
    "SLL",
    "SOS",
    "SRD",
    "SRG",
    "SSP",
    "STD",
    "STN",
    "SUR",
    "SVC",
    "SYP",
    "SZL",
    "THB",
    "TJR",
    "TJS",
    "TMM",
    "TMT",
    "TND",
    "TOP",
    "TPE",
    "TRL",
    "TRY",
    "TTD",
    "TWD",
    "TZS",
    "UAH",
    "UAK",
    "UGS",
    "UGW",
    "UGX",
    "USD",
    "USN",
    "USS",
    "UYI",
    "UYN",
    "UYP",
    "UYU",
    "UYW",
    "UZS",
    "VEB",
    "VEF",
    "VES",
    "VNC",
    "VND",
    "VUV",
    "WST",
    "XAF",
    "XAG",
    "XAU",
    "XBA",
    "XBB",
    "XBC",
    "XBD",
    "XCD",
    "XDR",
    "XEU",
    "XOF",
    "XPD",
    "XPF",
    "XPT",
    "XSU",
    "XTS",
    "XUA",
    "XXX",
    "YDD",
    "YER",
    "YUD",
    "YUM",
    "YUN",
    "ZAL",
    "ZAR",
    "ZMK",
    "ZMW",
    "ZRN",
    "ZRZ",
    "ZWC",
    "ZWD",
    "ZWL",
    "ZWN",
    "ZWR",
    "bit",
    "byte",
    "gigabit",
    "gigabyte",
    "kilobit",
    "kilobyte",
    "megabit",
    "megabyte",
    "petabyte",
    "terabit",
    "terabyte",
    "century",
    "day",
    "hour",
    "microsecond",
    "millisecond",
    "minute",
    "month",
    "nanosecond",
    "second",
    "week",
    "year",
    "ampere",
    "milliampere",
    "ohm",
    "volt",
    "calorie",
    "foodcalorie",
    "joule",
    "kilocalorie",
    "kilojoule",
    "kilowatt-hour",
    "gigahertz",
    "hertz",
    "kilohertz",
    "megahertz",
    "astronomical-unit",
    "centimeter",
    "decimeter",
    "fathom",
    "foot",
    "furlong",
    "inch",
    "kilometer",
    "light-year",
    "meter",
    "micrometer",
    "mile",
    "mile-scandinavian",
    "millimeter",
    "nanometer",
    "nautical-mile",
    "parsec",
    "picometer",
    "point",
    "yard",
    "lux",
    "carat",
    "gram",
    "kilogram",
    "metric-ton",
    "microgram",
    "milligram",
    "ounce",
    "ounce-troy",
    "pound",
    "stone",
    "ton",
    "base",
    "percent",
    "permille",
    "gigawatt",
    "horsepower",
    "kilowatt",
    "megawatt",
    "milliwatt",
    "watt",
    "atmosphere",
    "hectopascal",
    "inch-hg",
    "millibar",
    "millimeter-of-mercury",
    "pound-per-square-inch",
    "kilometer-per-hour",
    "knot",
    "meter-per-second",
    "mile-per-hour",
    "celsius",
    "fahrenheit",
    "generic",
    "kelvin",
    "acre-foot",
    "bushel",
    "centiliter",
    "cubic-centimeter",
    "cubic-foot",
    "cubic-inch",
    "cubic-kilometer",
    "cubic-meter",
    "cubic-mile",
    "cubic-yard",
    "cup",
    "cup-metric",
    "deciliter",
    "fluid-ounce",
    "gallon",
    "gallon-imperial",
    "hectoliter",
    "liter",
    "megaliter",
    "milliliter",
    "pint",
    "pint-metric",
    "quart",
    "tablespoon",
    "teaspoon"
};

// Must be sorted by first value and then second value.
static int32_t unitPerUnitToSingleUnit[][4] = {
        {368, 338, 17, 0},
        {370, 344, 17, 2},
        {372, 338, 17, 3},
        {372, 430, 4, 2},
        {372, 431, 4, 3},
        {387, 428, 3, 1},
        {390, 11, 16, 5},
        {433, 368, 4, 1}
};

// Shortcuts to the base unit in order to make the default constructor fast
static const int32_t kBaseTypeIdx = 14;
static const int32_t kBaseSubTypeIdx = 0;

MeasureUnit *MeasureUnit::createGForce(UErrorCode &status) {
    return MeasureUnit::create(0, 0, status);
}

MeasureUnit *MeasureUnit::createMeterPerSecondSquared(UErrorCode &status) {
    return MeasureUnit::create(0, 1, status);
}

MeasureUnit *MeasureUnit::createArcMinute(UErrorCode &status) {
    return MeasureUnit::create(1, 0, status);
}

MeasureUnit *MeasureUnit::createArcSecond(UErrorCode &status) {
    return MeasureUnit::create(1, 1, status);
}

MeasureUnit *MeasureUnit::createDegree(UErrorCode &status) {
    return MeasureUnit::create(1, 2, status);
}

MeasureUnit *MeasureUnit::createRadian(UErrorCode &status) {
    return MeasureUnit::create(1, 3, status);
}

MeasureUnit *MeasureUnit::createRevolutionAngle(UErrorCode &status) {
    return MeasureUnit::create(1, 4, status);
}

MeasureUnit *MeasureUnit::createAcre(UErrorCode &status) {
    return MeasureUnit::create(2, 0, status);
}

MeasureUnit *MeasureUnit::createHectare(UErrorCode &status) {
    return MeasureUnit::create(2, 1, status);
}

MeasureUnit *MeasureUnit::createSquareCentimeter(UErrorCode &status) {
    return MeasureUnit::create(2, 2, status);
}

MeasureUnit *MeasureUnit::createSquareFoot(UErrorCode &status) {
    return MeasureUnit::create(2, 3, status);
}

MeasureUnit *MeasureUnit::createSquareInch(UErrorCode &status) {
    return MeasureUnit::create(2, 4, status);
}

MeasureUnit *MeasureUnit::createSquareKilometer(UErrorCode &status) {
    return MeasureUnit::create(2, 5, status);
}

MeasureUnit *MeasureUnit::createSquareMeter(UErrorCode &status) {
    return MeasureUnit::create(2, 6, status);
}

MeasureUnit *MeasureUnit::createSquareMile(UErrorCode &status) {
    return MeasureUnit::create(2, 7, status);
}

MeasureUnit *MeasureUnit::createSquareYard(UErrorCode &status) {
    return MeasureUnit::create(2, 8, status);
}

MeasureUnit *MeasureUnit::createKarat(UErrorCode &status) {
    return MeasureUnit::create(3, 0, status);
}

MeasureUnit *MeasureUnit::createMilligramPerDeciliter(UErrorCode &status) {
    return MeasureUnit::create(3, 1, status);
}

MeasureUnit *MeasureUnit::createMillimolePerLiter(UErrorCode &status) {
    return MeasureUnit::create(3, 2, status);
}

MeasureUnit *MeasureUnit::createPartPerMillion(UErrorCode &status) {
    return MeasureUnit::create(3, 3, status);
}

MeasureUnit *MeasureUnit::createPercent(UErrorCode &status) {
    return MeasureUnit::create(3, 4, status);
}

MeasureUnit *MeasureUnit::createPermille(UErrorCode &status) {
    return MeasureUnit::create(3, 5, status);
}

MeasureUnit *MeasureUnit::createLiterPer100Kilometers(UErrorCode &status) {
    return MeasureUnit::create(4, 0, status);
}

MeasureUnit *MeasureUnit::createLiterPerKilometer(UErrorCode &status) {
    return MeasureUnit::create(4, 1, status);
}

MeasureUnit *MeasureUnit::createMilePerGallon(UErrorCode &status) {
    return MeasureUnit::create(4, 2, status);
}

MeasureUnit *MeasureUnit::createMilePerGallonImperial(UErrorCode &status) {
    return MeasureUnit::create(4, 3, status);
}

MeasureUnit *MeasureUnit::createBit(UErrorCode &status) {
    return MeasureUnit::create(6, 0, status);
}

MeasureUnit *MeasureUnit::createByte(UErrorCode &status) {
    return MeasureUnit::create(6, 1, status);
}

MeasureUnit *MeasureUnit::createGigabit(UErrorCode &status) {
    return MeasureUnit::create(6, 2, status);
}

MeasureUnit *MeasureUnit::createGigabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 3, status);
}

MeasureUnit *MeasureUnit::createKilobit(UErrorCode &status) {
    return MeasureUnit::create(6, 4, status);
}

MeasureUnit *MeasureUnit::createKilobyte(UErrorCode &status) {
    return MeasureUnit::create(6, 5, status);
}

MeasureUnit *MeasureUnit::createMegabit(UErrorCode &status) {
    return MeasureUnit::create(6, 6, status);
}

MeasureUnit *MeasureUnit::createMegabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 7, status);
}

MeasureUnit *MeasureUnit::createPetabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 8, status);
}

MeasureUnit *MeasureUnit::createTerabit(UErrorCode &status) {
    return MeasureUnit::create(6, 9, status);
}

MeasureUnit *MeasureUnit::createTerabyte(UErrorCode &status) {
    return MeasureUnit::create(6, 10, status);
}

MeasureUnit *MeasureUnit::createCentury(UErrorCode &status) {
    return MeasureUnit::create(7, 0, status);
}

MeasureUnit *MeasureUnit::createDay(UErrorCode &status) {
    return MeasureUnit::create(7, 1, status);
}

MeasureUnit *MeasureUnit::createHour(UErrorCode &status) {
    return MeasureUnit::create(7, 2, status);
}

MeasureUnit *MeasureUnit::createMicrosecond(UErrorCode &status) {
    return MeasureUnit::create(7, 3, status);
}

MeasureUnit *MeasureUnit::createMillisecond(UErrorCode &status) {
    return MeasureUnit::create(7, 4, status);
}

MeasureUnit *MeasureUnit::createMinute(UErrorCode &status) {
    return MeasureUnit::create(7, 5, status);
}

MeasureUnit *MeasureUnit::createMonth(UErrorCode &status) {
    return MeasureUnit::create(7, 6, status);
}

MeasureUnit *MeasureUnit::createNanosecond(UErrorCode &status) {
    return MeasureUnit::create(7, 7, status);
}

MeasureUnit *MeasureUnit::createSecond(UErrorCode &status) {
    return MeasureUnit::create(7, 8, status);
}

MeasureUnit *MeasureUnit::createWeek(UErrorCode &status) {
    return MeasureUnit::create(7, 9, status);
}

MeasureUnit *MeasureUnit::createYear(UErrorCode &status) {
    return MeasureUnit::create(7, 10, status);
}

MeasureUnit *MeasureUnit::createAmpere(UErrorCode &status) {
    return MeasureUnit::create(8, 0, status);
}

MeasureUnit *MeasureUnit::createMilliampere(UErrorCode &status) {
    return MeasureUnit::create(8, 1, status);
}

MeasureUnit *MeasureUnit::createOhm(UErrorCode &status) {
    return MeasureUnit::create(8, 2, status);
}

MeasureUnit *MeasureUnit::createVolt(UErrorCode &status) {
    return MeasureUnit::create(8, 3, status);
}

MeasureUnit *MeasureUnit::createCalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 0, status);
}

MeasureUnit *MeasureUnit::createFoodcalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 1, status);
}

MeasureUnit *MeasureUnit::createJoule(UErrorCode &status) {
    return MeasureUnit::create(9, 2, status);
}

MeasureUnit *MeasureUnit::createKilocalorie(UErrorCode &status) {
    return MeasureUnit::create(9, 3, status);
}

MeasureUnit *MeasureUnit::createKilojoule(UErrorCode &status) {
    return MeasureUnit::create(9, 4, status);
}

MeasureUnit *MeasureUnit::createKilowattHour(UErrorCode &status) {
    return MeasureUnit::create(9, 5, status);
}

MeasureUnit *MeasureUnit::createGigahertz(UErrorCode &status) {
    return MeasureUnit::create(10, 0, status);
}

MeasureUnit *MeasureUnit::createHertz(UErrorCode &status) {
    return MeasureUnit::create(10, 1, status);
}

MeasureUnit *MeasureUnit::createKilohertz(UErrorCode &status) {
    return MeasureUnit::create(10, 2, status);
}

MeasureUnit *MeasureUnit::createMegahertz(UErrorCode &status) {
    return MeasureUnit::create(10, 3, status);
}

MeasureUnit *MeasureUnit::createAstronomicalUnit(UErrorCode &status) {
    return MeasureUnit::create(11, 0, status);
}

MeasureUnit *MeasureUnit::createCentimeter(UErrorCode &status) {
    return MeasureUnit::create(11, 1, status);
}

MeasureUnit *MeasureUnit::createDecimeter(UErrorCode &status) {
    return MeasureUnit::create(11, 2, status);
}

MeasureUnit *MeasureUnit::createFathom(UErrorCode &status) {
    return MeasureUnit::create(11, 3, status);
}

MeasureUnit *MeasureUnit::createFoot(UErrorCode &status) {
    return MeasureUnit::create(11, 4, status);
}

MeasureUnit *MeasureUnit::createFurlong(UErrorCode &status) {
    return MeasureUnit::create(11, 5, status);
}

MeasureUnit *MeasureUnit::createInch(UErrorCode &status) {
    return MeasureUnit::create(11, 6, status);
}

MeasureUnit *MeasureUnit::createKilometer(UErrorCode &status) {
    return MeasureUnit::create(11, 7, status);
}

MeasureUnit *MeasureUnit::createLightYear(UErrorCode &status) {
    return MeasureUnit::create(11, 8, status);
}

MeasureUnit *MeasureUnit::createMeter(UErrorCode &status) {
    return MeasureUnit::create(11, 9, status);
}

MeasureUnit *MeasureUnit::createMicrometer(UErrorCode &status) {
    return MeasureUnit::create(11, 10, status);
}

MeasureUnit *MeasureUnit::createMile(UErrorCode &status) {
    return MeasureUnit::create(11, 11, status);
}

MeasureUnit *MeasureUnit::createMileScandinavian(UErrorCode &status) {
    return MeasureUnit::create(11, 12, status);
}

MeasureUnit *MeasureUnit::createMillimeter(UErrorCode &status) {
    return MeasureUnit::create(11, 13, status);
}

MeasureUnit *MeasureUnit::createNanometer(UErrorCode &status) {
    return MeasureUnit::create(11, 14, status);
}

MeasureUnit *MeasureUnit::createNauticalMile(UErrorCode &status) {
    return MeasureUnit::create(11, 15, status);
}

MeasureUnit *MeasureUnit::createParsec(UErrorCode &status) {
    return MeasureUnit::create(11, 16, status);
}

MeasureUnit *MeasureUnit::createPicometer(UErrorCode &status) {
    return MeasureUnit::create(11, 17, status);
}

MeasureUnit *MeasureUnit::createPoint(UErrorCode &status) {
    return MeasureUnit::create(11, 18, status);
}

MeasureUnit *MeasureUnit::createYard(UErrorCode &status) {
    return MeasureUnit::create(11, 19, status);
}

MeasureUnit *MeasureUnit::createLux(UErrorCode &status) {
    return MeasureUnit::create(12, 0, status);
}

MeasureUnit *MeasureUnit::createCarat(UErrorCode &status) {
    return MeasureUnit::create(13, 0, status);
}

MeasureUnit *MeasureUnit::createGram(UErrorCode &status) {
    return MeasureUnit::create(13, 1, status);
}

MeasureUnit *MeasureUnit::createKilogram(UErrorCode &status) {
    return MeasureUnit::create(13, 2, status);
}

MeasureUnit *MeasureUnit::createMetricTon(UErrorCode &status) {
    return MeasureUnit::create(13, 3, status);
}

MeasureUnit *MeasureUnit::createMicrogram(UErrorCode &status) {
    return MeasureUnit::create(13, 4, status);
}

MeasureUnit *MeasureUnit::createMilligram(UErrorCode &status) {
    return MeasureUnit::create(13, 5, status);
}

MeasureUnit *MeasureUnit::createOunce(UErrorCode &status) {
    return MeasureUnit::create(13, 6, status);
}

MeasureUnit *MeasureUnit::createOunceTroy(UErrorCode &status) {
    return MeasureUnit::create(13, 7, status);
}

MeasureUnit *MeasureUnit::createPound(UErrorCode &status) {
    return MeasureUnit::create(13, 8, status);
}

MeasureUnit *MeasureUnit::createStone(UErrorCode &status) {
    return MeasureUnit::create(13, 9, status);
}

MeasureUnit *MeasureUnit::createTon(UErrorCode &status) {
    return MeasureUnit::create(13, 10, status);
}

MeasureUnit *MeasureUnit::createGigawatt(UErrorCode &status) {
    return MeasureUnit::create(15, 0, status);
}

MeasureUnit *MeasureUnit::createHorsepower(UErrorCode &status) {
    return MeasureUnit::create(15, 1, status);
}

MeasureUnit *MeasureUnit::createKilowatt(UErrorCode &status) {
    return MeasureUnit::create(15, 2, status);
}

MeasureUnit *MeasureUnit::createMegawatt(UErrorCode &status) {
    return MeasureUnit::create(15, 3, status);
}

MeasureUnit *MeasureUnit::createMilliwatt(UErrorCode &status) {
    return MeasureUnit::create(15, 4, status);
}

MeasureUnit *MeasureUnit::createWatt(UErrorCode &status) {
    return MeasureUnit::create(15, 5, status);
}

MeasureUnit *MeasureUnit::createAtmosphere(UErrorCode &status) {
    return MeasureUnit::create(16, 0, status);
}

MeasureUnit *MeasureUnit::createHectopascal(UErrorCode &status) {
    return MeasureUnit::create(16, 1, status);
}

MeasureUnit *MeasureUnit::createInchHg(UErrorCode &status) {
    return MeasureUnit::create(16, 2, status);
}

MeasureUnit *MeasureUnit::createMillibar(UErrorCode &status) {
    return MeasureUnit::create(16, 3, status);
}

MeasureUnit *MeasureUnit::createMillimeterOfMercury(UErrorCode &status) {
    return MeasureUnit::create(16, 4, status);
}

MeasureUnit *MeasureUnit::createPoundPerSquareInch(UErrorCode &status) {
    return MeasureUnit::create(16, 5, status);
}

MeasureUnit *MeasureUnit::createKilometerPerHour(UErrorCode &status) {
    return MeasureUnit::create(17, 0, status);
}

MeasureUnit *MeasureUnit::createKnot(UErrorCode &status) {
    return MeasureUnit::create(17, 1, status);
}

MeasureUnit *MeasureUnit::createMeterPerSecond(UErrorCode &status) {
    return MeasureUnit::create(17, 2, status);
}

MeasureUnit *MeasureUnit::createMilePerHour(UErrorCode &status) {
    return MeasureUnit::create(17, 3, status);
}

MeasureUnit *MeasureUnit::createCelsius(UErrorCode &status) {
    return MeasureUnit::create(18, 0, status);
}

MeasureUnit *MeasureUnit::createFahrenheit(UErrorCode &status) {
    return MeasureUnit::create(18, 1, status);
}

MeasureUnit *MeasureUnit::createGenericTemperature(UErrorCode &status) {
    return MeasureUnit::create(18, 2, status);
}

MeasureUnit *MeasureUnit::createKelvin(UErrorCode &status) {
    return MeasureUnit::create(18, 3, status);
}

MeasureUnit *MeasureUnit::createAcreFoot(UErrorCode &status) {
    return MeasureUnit::create(19, 0, status);
}

MeasureUnit *MeasureUnit::createBushel(UErrorCode &status) {
    return MeasureUnit::create(19, 1, status);
}

MeasureUnit *MeasureUnit::createCentiliter(UErrorCode &status) {
    return MeasureUnit::create(19, 2, status);
}

MeasureUnit *MeasureUnit::createCubicCentimeter(UErrorCode &status) {
    return MeasureUnit::create(19, 3, status);
}

MeasureUnit *MeasureUnit::createCubicFoot(UErrorCode &status) {
    return MeasureUnit::create(19, 4, status);
}

MeasureUnit *MeasureUnit::createCubicInch(UErrorCode &status) {
    return MeasureUnit::create(19, 5, status);
}

MeasureUnit *MeasureUnit::createCubicKilometer(UErrorCode &status) {
    return MeasureUnit::create(19, 6, status);
}

MeasureUnit *MeasureUnit::createCubicMeter(UErrorCode &status) {
    return MeasureUnit::create(19, 7, status);
}

MeasureUnit *MeasureUnit::createCubicMile(UErrorCode &status) {
    return MeasureUnit::create(19, 8, status);
}

MeasureUnit *MeasureUnit::createCubicYard(UErrorCode &status) {
    return MeasureUnit::create(19, 9, status);
}

MeasureUnit *MeasureUnit::createCup(UErrorCode &status) {
    return MeasureUnit::create(19, 10, status);
}

MeasureUnit *MeasureUnit::createCupMetric(UErrorCode &status) {
    return MeasureUnit::create(19, 11, status);
}

MeasureUnit *MeasureUnit::createDeciliter(UErrorCode &status) {
    return MeasureUnit::create(19, 12, status);
}

MeasureUnit *MeasureUnit::createFluidOunce(UErrorCode &status) {
    return MeasureUnit::create(19, 13, status);
}

MeasureUnit *MeasureUnit::createGallon(UErrorCode &status) {
    return MeasureUnit::create(19, 14, status);
}

MeasureUnit *MeasureUnit::createGallonImperial(UErrorCode &status) {
    return MeasureUnit::create(19, 15, status);
}

MeasureUnit *MeasureUnit::createHectoliter(UErrorCode &status) {
    return MeasureUnit::create(19, 16, status);
}

MeasureUnit *MeasureUnit::createLiter(UErrorCode &status) {
    return MeasureUnit::create(19, 17, status);
}

MeasureUnit *MeasureUnit::createMegaliter(UErrorCode &status) {
    return MeasureUnit::create(19, 18, status);
}

MeasureUnit *MeasureUnit::createMilliliter(UErrorCode &status) {
    return MeasureUnit::create(19, 19, status);
}

MeasureUnit *MeasureUnit::createPint(UErrorCode &status) {
    return MeasureUnit::create(19, 20, status);
}

MeasureUnit *MeasureUnit::createPintMetric(UErrorCode &status) {
    return MeasureUnit::create(19, 21, status);
}

MeasureUnit *MeasureUnit::createQuart(UErrorCode &status) {
    return MeasureUnit::create(19, 22, status);
}

MeasureUnit *MeasureUnit::createTablespoon(UErrorCode &status) {
    return MeasureUnit::create(19, 23, status);
}

MeasureUnit *MeasureUnit::createTeaspoon(UErrorCode &status) {
    return MeasureUnit::create(19, 24, status);
}

// End generated code

static int32_t binarySearch(
        const char * const * array, int32_t start, int32_t end, const char * key) {
    while (start < end) {
        int32_t mid = (start + end) / 2;
        int32_t cmp = uprv_strcmp(array[mid], key);
        if (cmp < 0) {
            start = mid + 1;
            continue;
        }
        if (cmp == 0) {
            return mid;
        }
        end = mid;
    }
    return -1;
}

MeasureUnit::MeasureUnit() {
    fCurrency[0] = 0;
    fTypeId = kBaseTypeIdx;
    fSubTypeId = kBaseSubTypeIdx;
}

MeasureUnit::MeasureUnit(const MeasureUnit &other)
        : fTypeId(other.fTypeId), fSubTypeId(other.fSubTypeId) {
    uprv_strcpy(fCurrency, other.fCurrency);
}

MeasureUnit &MeasureUnit::operator=(const MeasureUnit &other) {
    if (this == &other) {
        return *this;
    }
    fTypeId = other.fTypeId;
    fSubTypeId = other.fSubTypeId;
    uprv_strcpy(fCurrency, other.fCurrency);
    return *this;
}

UObject *MeasureUnit::clone() const {
    return new MeasureUnit(*this);
}

MeasureUnit::~MeasureUnit() {
}

const char *MeasureUnit::getType() const {
    return gTypes[fTypeId];
}

const char *MeasureUnit::getSubtype() const {
    return fCurrency[0] == 0 ? gSubTypes[getOffset()] : fCurrency;
}

UBool MeasureUnit::operator==(const UObject& other) const {
    if (this == &other) {  // Same object, equal
        return TRUE;
    }
    if (typeid(*this) != typeid(other)) { // Different types, not equal
        return FALSE;
    }
    const MeasureUnit &rhs = static_cast<const MeasureUnit&>(other);
    return (
            fTypeId == rhs.fTypeId
            && fSubTypeId == rhs.fSubTypeId
            && uprv_strcmp(fCurrency, rhs.fCurrency) == 0);
}

int32_t MeasureUnit::getIndex() const {
    return gIndexes[fTypeId] + fSubTypeId;
}

int32_t MeasureUnit::getAvailable(
        MeasureUnit *dest,
        int32_t destCapacity,
        UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    if (destCapacity < UPRV_LENGTHOF(gSubTypes)) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return UPRV_LENGTHOF(gSubTypes);
    }
    int32_t idx = 0;
    for (int32_t typeIdx = 0; typeIdx < UPRV_LENGTHOF(gTypes); ++typeIdx) {
        int32_t len = gOffsets[typeIdx + 1] - gOffsets[typeIdx];
        for (int32_t subTypeIdx = 0; subTypeIdx < len; ++subTypeIdx) {
            dest[idx].setTo(typeIdx, subTypeIdx);
            ++idx;
        }
    }
    U_ASSERT(idx == UPRV_LENGTHOF(gSubTypes));
    return UPRV_LENGTHOF(gSubTypes);
}

int32_t MeasureUnit::getAvailable(
        const char *type,
        MeasureUnit *dest,
        int32_t destCapacity,
        UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    int32_t typeIdx = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), type);
    if (typeIdx == -1) {
        return 0;
    }
    int32_t len = gOffsets[typeIdx + 1] - gOffsets[typeIdx];
    if (destCapacity < len) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return len;
    }
    for (int subTypeIdx = 0; subTypeIdx < len; ++subTypeIdx) {
        dest[subTypeIdx].setTo(typeIdx, subTypeIdx);
    }
    return len;
}

StringEnumeration* MeasureUnit::getAvailableTypes(UErrorCode &errorCode) {
    UEnumeration *uenum = uenum_openCharStringsEnumeration(
            gTypes, UPRV_LENGTHOF(gTypes), &errorCode);
    if (U_FAILURE(errorCode)) {
        uenum_close(uenum);
        return NULL;
    }
    StringEnumeration *result = new UStringEnumeration(uenum);
    if (result == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        uenum_close(uenum);
        return NULL;
    }
    return result;
}

int32_t MeasureUnit::getIndexCount() {
    return gIndexes[UPRV_LENGTHOF(gIndexes) - 1];
}

int32_t MeasureUnit::internalGetIndexForTypeAndSubtype(const char *type, const char *subtype) {
    int32_t t = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), type);
    if (t < 0) {
        return t;
    }
    int32_t st = binarySearch(gSubTypes, gOffsets[t], gOffsets[t + 1], subtype);
    if (st < 0) {
        return st;
    }
    return gIndexes[t] + st - gOffsets[t];
}

MeasureUnit MeasureUnit::resolveUnitPerUnit(
        const MeasureUnit &unit, const MeasureUnit &perUnit, bool* isResolved) {
    int32_t unitOffset = unit.getOffset();
    int32_t perUnitOffset = perUnit.getOffset();

    // binary search for (unitOffset, perUnitOffset)
    int32_t start = 0;
    int32_t end = UPRV_LENGTHOF(unitPerUnitToSingleUnit);
    while (start < end) {
        int32_t mid = (start + end) / 2;
        int32_t *midRow = unitPerUnitToSingleUnit[mid];
        if (unitOffset < midRow[0]) {
            end = mid;
        } else if (unitOffset > midRow[0]) {
            start = mid + 1;
        } else if (perUnitOffset < midRow[1]) {
            end = mid;
        } else if (perUnitOffset > midRow[1]) {
            start = mid + 1;
        } else {
            // We found a resolution for our unit / per-unit combo
            // return it.
            *isResolved = true;
            return MeasureUnit(midRow[2], midRow[3]);
        }
    }

    *isResolved = false;
    return MeasureUnit();
}

MeasureUnit *MeasureUnit::create(int typeId, int subTypeId, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    MeasureUnit *result = new MeasureUnit(typeId, subTypeId);
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

void MeasureUnit::initTime(const char *timeId) {
    int32_t result = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), "duration");
    U_ASSERT(result != -1);
    fTypeId = result;
    result = binarySearch(gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], timeId);
    U_ASSERT(result != -1);
    fSubTypeId = result - gOffsets[fTypeId];
}

void MeasureUnit::initCurrency(const char *isoCurrency) {
    int32_t result = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), "currency");
    U_ASSERT(result != -1);
    fTypeId = result;
    result = binarySearch(
            gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], isoCurrency);
    if (result != -1) {
        fSubTypeId = result - gOffsets[fTypeId];
    } else {
        uprv_strncpy(fCurrency, isoCurrency, UPRV_LENGTHOF(fCurrency));
        fCurrency[3] = 0;
    }
}

void MeasureUnit::initNoUnit(const char *subtype) {
    int32_t result = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), "none");
    U_ASSERT(result != -1);
    fTypeId = result;
    result = binarySearch(gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], subtype);
    U_ASSERT(result != -1);
    fSubTypeId = result - gOffsets[fTypeId];
}

void MeasureUnit::setTo(int32_t typeId, int32_t subTypeId) {
    fTypeId = typeId;
    fSubTypeId = subTypeId;
    fCurrency[0] = 0;
}

int32_t MeasureUnit::getOffset() const {
    return gOffsets[fTypeId] + fSubTypeId;
}

U_NAMESPACE_END

#endif /* !UNCONFIG_NO_FORMATTING */
