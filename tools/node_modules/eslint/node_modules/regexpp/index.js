/*! @author Toru Nagashima <https://github.com/mysticatea> */
'use strict';

Object.defineProperty(exports, '__esModule', { value: true });



var ast = /*#__PURE__*/Object.freeze({

});

let largeIdStartPattern = null;
let largeIdContinuePattern = null;
function isIdStart(cp) {
    if (cp < 0x41)
        return false;
    if (cp < 0x5b)
        return true;
    if (cp < 0x61)
        return false;
    if (cp < 0x7b)
        return true;
    return isLargeIdStart(cp);
}
function isIdContinue(cp) {
    if (cp < 0x30)
        return false;
    if (cp < 0x3a)
        return true;
    if (cp < 0x41)
        return false;
    if (cp < 0x5b)
        return true;
    if (cp === 0x5f)
        return true;
    if (cp < 0x61)
        return false;
    if (cp < 0x7b)
        return true;
    return isLargeIdStart(cp) || isLargeIdContinue(cp);
}
function isLargeIdStart(cp) {
    if (!largeIdStartPattern) {
        largeIdStartPattern = new RegExp("^[\xaa\xb5\xba\xc0-\xd6\xd8-\xf6\xf8-\u02c1\u02c6-\u02d1" +
            "\u02e0-\u02e4\u02ec\u02ee\u0370-\u0374\u0376\u0377" +
            "\u037a-\u037d\u037f\u0386\u0388-\u038a\u038c\u038e-\u03a1" +
            "\u03a3-\u03f5\u03f7-\u0481\u048a-\u052f\u0531-\u0556\u0559" +
            "\u0560-\u0588\u05d0-\u05ea\u05ef-\u05f2\u0620-\u064a" +
            "\u066e\u066f\u0671-\u06d3\u06d5\u06e5\u06e6\u06ee\u06ef" +
            "\u06fa-\u06fc\u06ff\u0710\u0712-\u072f\u074d-\u07a5\u07b1" +
            "\u07ca-\u07ea\u07f4\u07f5\u07fa\u0800-\u0815\u081a\u0824" +
            "\u0828\u0840-\u0858\u0860-\u086a\u08a0-\u08b4\u08b6-\u08bd" +
            "\u0904-\u0939\u093d\u0950\u0958-\u0961\u0971-\u0980" +
            "\u0985-\u098c\u098f\u0990\u0993-\u09a8\u09aa-\u09b0\u09b2" +
            "\u09b6-\u09b9\u09bd\u09ce\u09dc\u09dd\u09df-\u09e1" +
            "\u09f0\u09f1\u09fc\u0a05-\u0a0a\u0a0f\u0a10\u0a13-\u0a28" +
            "\u0a2a-\u0a30\u0a32\u0a33\u0a35\u0a36\u0a38\u0a39" +
            "\u0a59-\u0a5c\u0a5e\u0a72-\u0a74\u0a85-\u0a8d\u0a8f-\u0a91" +
            "\u0a93-\u0aa8\u0aaa-\u0ab0\u0ab2\u0ab3\u0ab5-\u0ab9\u0abd" +
            "\u0ad0\u0ae0\u0ae1\u0af9\u0b05-\u0b0c\u0b0f\u0b10" +
            "\u0b13-\u0b28\u0b2a-\u0b30\u0b32\u0b33\u0b35-\u0b39\u0b3d" +
            "\u0b5c\u0b5d\u0b5f-\u0b61\u0b71\u0b83\u0b85-\u0b8a" +
            "\u0b8e-\u0b90\u0b92-\u0b95\u0b99\u0b9a\u0b9c\u0b9e\u0b9f" +
            "\u0ba3\u0ba4\u0ba8-\u0baa\u0bae-\u0bb9\u0bd0\u0c05-\u0c0c" +
            "\u0c0e-\u0c10\u0c12-\u0c28\u0c2a-\u0c39\u0c3d\u0c58-\u0c5a" +
            "\u0c60\u0c61\u0c80\u0c85-\u0c8c\u0c8e-\u0c90\u0c92-\u0ca8" +
            "\u0caa-\u0cb3\u0cb5-\u0cb9\u0cbd\u0cde\u0ce0\u0ce1" +
            "\u0cf1\u0cf2\u0d05-\u0d0c\u0d0e-\u0d10\u0d12-\u0d3a\u0d3d" +
            "\u0d4e\u0d54-\u0d56\u0d5f-\u0d61\u0d7a-\u0d7f\u0d85-\u0d96" +
            "\u0d9a-\u0db1\u0db3-\u0dbb\u0dbd\u0dc0-\u0dc6\u0e01-\u0e30" +
            "\u0e32\u0e33\u0e40-\u0e46\u0e81\u0e82\u0e84\u0e86-\u0e8a" +
            "\u0e8c-\u0ea3\u0ea5\u0ea7-\u0eb0\u0eb2\u0eb3\u0ebd" +
            "\u0ec0-\u0ec4\u0ec6\u0edc-\u0edf\u0f00\u0f40-\u0f47" +
            "\u0f49-\u0f6c\u0f88-\u0f8c\u1000-\u102a\u103f\u1050-\u1055" +
            "\u105a-\u105d\u1061\u1065\u1066\u106e-\u1070\u1075-\u1081" +
            "\u108e\u10a0-\u10c5\u10c7\u10cd\u10d0-\u10fa\u10fc-\u1248" +
            "\u124a-\u124d\u1250-\u1256\u1258\u125a-\u125d\u1260-\u1288" +
            "\u128a-\u128d\u1290-\u12b0\u12b2-\u12b5\u12b8-\u12be\u12c0" +
            "\u12c2-\u12c5\u12c8-\u12d6\u12d8-\u1310\u1312-\u1315" +
            "\u1318-\u135a\u1380-\u138f\u13a0-\u13f5\u13f8-\u13fd" +
            "\u1401-\u166c\u166f-\u167f\u1681-\u169a\u16a0-\u16ea" +
            "\u16ee-\u16f8\u1700-\u170c\u170e-\u1711\u1720-\u1731" +
            "\u1740-\u1751\u1760-\u176c\u176e-\u1770\u1780-\u17b3\u17d7" +
            "\u17dc\u1820-\u1878\u1880-\u18a8\u18aa\u18b0-\u18f5" +
            "\u1900-\u191e\u1950-\u196d\u1970-\u1974\u1980-\u19ab" +
            "\u19b0-\u19c9\u1a00-\u1a16\u1a20-\u1a54\u1aa7\u1b05-\u1b33" +
            "\u1b45-\u1b4b\u1b83-\u1ba0\u1bae\u1baf\u1bba-\u1be5" +
            "\u1c00-\u1c23\u1c4d-\u1c4f\u1c5a-\u1c7d\u1c80-\u1c88" +
            "\u1c90-\u1cba\u1cbd-\u1cbf\u1ce9-\u1cec\u1cee-\u1cf3" +
            "\u1cf5\u1cf6\u1cfa\u1d00-\u1dbf\u1e00-\u1f15\u1f18-\u1f1d" +
            "\u1f20-\u1f45\u1f48-\u1f4d\u1f50-\u1f57\u1f59\u1f5b\u1f5d" +
            "\u1f5f-\u1f7d\u1f80-\u1fb4\u1fb6-\u1fbc\u1fbe\u1fc2-\u1fc4" +
            "\u1fc6-\u1fcc\u1fd0-\u1fd3\u1fd6-\u1fdb\u1fe0-\u1fec" +
            "\u1ff2-\u1ff4\u1ff6-\u1ffc\u2071\u207f\u2090-\u209c\u2102" +
            "\u2107\u210a-\u2113\u2115\u2118-\u211d\u2124\u2126\u2128" +
            "\u212a-\u2139\u213c-\u213f\u2145-\u2149\u214e\u2160-\u2188" +
            "\u2c00-\u2c2e\u2c30-\u2c5e\u2c60-\u2ce4\u2ceb-\u2cee" +
            "\u2cf2\u2cf3\u2d00-\u2d25\u2d27\u2d2d\u2d30-\u2d67\u2d6f" +
            "\u2d80-\u2d96\u2da0-\u2da6\u2da8-\u2dae\u2db0-\u2db6" +
            "\u2db8-\u2dbe\u2dc0-\u2dc6\u2dc8-\u2dce\u2dd0-\u2dd6" +
            "\u2dd8-\u2dde\u3005-\u3007\u3021-\u3029\u3031-\u3035" +
            "\u3038-\u303c\u3041-\u3096\u309b-\u309f\u30a1-\u30fa" +
            "\u30fc-\u30ff\u3105-\u312f\u3131-\u318e\u31a0-\u31ba" +
            "\u31f0-\u31ff\u3400-\u4db5\u4e00-\u9fef\ua000-\ua48c" +
            "\ua4d0-\ua4fd\ua500-\ua60c\ua610-\ua61f\ua62a\ua62b" +
            "\ua640-\ua66e\ua67f-\ua69d\ua6a0-\ua6ef\ua717-\ua71f" +
            "\ua722-\ua788\ua78b-\ua7bf\ua7c2-\ua7c6\ua7f7-\ua801" +
            "\ua803-\ua805\ua807-\ua80a\ua80c-\ua822\ua840-\ua873" +
            "\ua882-\ua8b3\ua8f2-\ua8f7\ua8fb\ua8fd\ua8fe\ua90a-\ua925" +
            "\ua930-\ua946\ua960-\ua97c\ua984-\ua9b2\ua9cf\ua9e0-\ua9e4" +
            "\ua9e6-\ua9ef\ua9fa-\ua9fe\uaa00-\uaa28\uaa40-\uaa42" +
            "\uaa44-\uaa4b\uaa60-\uaa76\uaa7a\uaa7e-\uaaaf\uaab1" +
            "\uaab5\uaab6\uaab9-\uaabd\uaac0\uaac2\uaadb-\uaadd" +
            "\uaae0-\uaaea\uaaf2-\uaaf4\uab01-\uab06\uab09-\uab0e" +
            "\uab11-\uab16\uab20-\uab26\uab28-\uab2e\uab30-\uab5a" +
            "\uab5c-\uab67\uab70-\uabe2\uac00-\ud7a3\ud7b0-\ud7c6" +
            "\ud7cb-\ud7fb\uf900-\ufa6d\ufa70-\ufad9\ufb00-\ufb06" +
            "\ufb13-\ufb17\ufb1d\ufb1f-\ufb28\ufb2a-\ufb36\ufb38-\ufb3c" +
            "\ufb3e\ufb40\ufb41\ufb43\ufb44\ufb46-\ufbb1\ufbd3-\ufd3d" +
            "\ufd50-\ufd8f\ufd92-\ufdc7\ufdf0-\ufdfb\ufe70-\ufe74" +
            "\ufe76-\ufefc\uff21-\uff3a\uff41-\uff5a\uff66-\uffbe" +
            "\uffc2-\uffc7\uffca-\uffcf\uffd2-\uffd7\uffda-\uffdc" +
            "\u{10000}-\u{1000b}\u{1000d}-\u{10026}\u{10028}-\u{1003a}" +
            "\u{1003c}\u{1003d}\u{1003f}-\u{1004d}\u{10050}-\u{1005d}" +
            "\u{10080}-\u{100fa}\u{10140}-\u{10174}\u{10280}-\u{1029c}" +
            "\u{102a0}-\u{102d0}\u{10300}-\u{1031f}\u{1032d}-\u{1034a}" +
            "\u{10350}-\u{10375}\u{10380}-\u{1039d}\u{103a0}-\u{103c3}" +
            "\u{103c8}-\u{103cf}\u{103d1}-\u{103d5}\u{10400}-\u{1049d}" +
            "\u{104b0}-\u{104d3}\u{104d8}-\u{104fb}\u{10500}-\u{10527}" +
            "\u{10530}-\u{10563}\u{10600}-\u{10736}\u{10740}-\u{10755}" +
            "\u{10760}-\u{10767}\u{10800}-\u{10805}\u{10808}" +
            "\u{1080a}-\u{10835}\u{10837}\u{10838}\u{1083c}" +
            "\u{1083f}-\u{10855}\u{10860}-\u{10876}\u{10880}-\u{1089e}" +
            "\u{108e0}-\u{108f2}\u{108f4}\u{108f5}\u{10900}-\u{10915}" +
            "\u{10920}-\u{10939}\u{10980}-\u{109b7}\u{109be}\u{109bf}" +
            "\u{10a00}\u{10a10}-\u{10a13}\u{10a15}-\u{10a17}" +
            "\u{10a19}-\u{10a35}\u{10a60}-\u{10a7c}\u{10a80}-\u{10a9c}" +
            "\u{10ac0}-\u{10ac7}\u{10ac9}-\u{10ae4}\u{10b00}-\u{10b35}" +
            "\u{10b40}-\u{10b55}\u{10b60}-\u{10b72}\u{10b80}-\u{10b91}" +
            "\u{10c00}-\u{10c48}\u{10c80}-\u{10cb2}\u{10cc0}-\u{10cf2}" +
            "\u{10d00}-\u{10d23}\u{10f00}-\u{10f1c}\u{10f27}" +
            "\u{10f30}-\u{10f45}\u{10fe0}-\u{10ff6}\u{11003}-\u{11037}" +
            "\u{11083}-\u{110af}\u{110d0}-\u{110e8}\u{11103}-\u{11126}" +
            "\u{11144}\u{11150}-\u{11172}\u{11176}\u{11183}-\u{111b2}" +
            "\u{111c1}-\u{111c4}\u{111da}\u{111dc}\u{11200}-\u{11211}" +
            "\u{11213}-\u{1122b}\u{11280}-\u{11286}\u{11288}" +
            "\u{1128a}-\u{1128d}\u{1128f}-\u{1129d}\u{1129f}-\u{112a8}" +
            "\u{112b0}-\u{112de}\u{11305}-\u{1130c}\u{1130f}\u{11310}" +
            "\u{11313}-\u{11328}\u{1132a}-\u{11330}\u{11332}\u{11333}" +
            "\u{11335}-\u{11339}\u{1133d}\u{11350}\u{1135d}-\u{11361}" +
            "\u{11400}-\u{11434}\u{11447}-\u{1144a}\u{1145f}" +
            "\u{11480}-\u{114af}\u{114c4}\u{114c5}\u{114c7}" +
            "\u{11580}-\u{115ae}\u{115d8}-\u{115db}\u{11600}-\u{1162f}" +
            "\u{11644}\u{11680}-\u{116aa}\u{116b8}\u{11700}-\u{1171a}" +
            "\u{11800}-\u{1182b}\u{118a0}-\u{118df}\u{118ff}" +
            "\u{119a0}-\u{119a7}\u{119aa}-\u{119d0}\u{119e1}\u{119e3}" +
            "\u{11a00}\u{11a0b}-\u{11a32}\u{11a3a}\u{11a50}" +
            "\u{11a5c}-\u{11a89}\u{11a9d}\u{11ac0}-\u{11af8}" +
            "\u{11c00}-\u{11c08}\u{11c0a}-\u{11c2e}\u{11c40}" +
            "\u{11c72}-\u{11c8f}\u{11d00}-\u{11d06}\u{11d08}\u{11d09}" +
            "\u{11d0b}-\u{11d30}\u{11d46}\u{11d60}-\u{11d65}" +
            "\u{11d67}\u{11d68}\u{11d6a}-\u{11d89}\u{11d98}" +
            "\u{11ee0}-\u{11ef2}\u{12000}-\u{12399}\u{12400}-\u{1246e}" +
            "\u{12480}-\u{12543}\u{13000}-\u{1342e}\u{14400}-\u{14646}" +
            "\u{16800}-\u{16a38}\u{16a40}-\u{16a5e}\u{16ad0}-\u{16aed}" +
            "\u{16b00}-\u{16b2f}\u{16b40}-\u{16b43}\u{16b63}-\u{16b77}" +
            "\u{16b7d}-\u{16b8f}\u{16e40}-\u{16e7f}\u{16f00}-\u{16f4a}" +
            "\u{16f50}\u{16f93}-\u{16f9f}\u{16fe0}\u{16fe1}\u{16fe3}" +
            "\u{17000}-\u{187f7}\u{18800}-\u{18af2}\u{1b000}-\u{1b11e}" +
            "\u{1b150}-\u{1b152}\u{1b164}-\u{1b167}\u{1b170}-\u{1b2fb}" +
            "\u{1bc00}-\u{1bc6a}\u{1bc70}-\u{1bc7c}\u{1bc80}-\u{1bc88}" +
            "\u{1bc90}-\u{1bc99}\u{1d400}-\u{1d454}\u{1d456}-\u{1d49c}" +
            "\u{1d49e}\u{1d49f}\u{1d4a2}\u{1d4a5}\u{1d4a6}" +
            "\u{1d4a9}-\u{1d4ac}\u{1d4ae}-\u{1d4b9}\u{1d4bb}" +
            "\u{1d4bd}-\u{1d4c3}\u{1d4c5}-\u{1d505}\u{1d507}-\u{1d50a}" +
            "\u{1d50d}-\u{1d514}\u{1d516}-\u{1d51c}\u{1d51e}-\u{1d539}" +
            "\u{1d53b}-\u{1d53e}\u{1d540}-\u{1d544}\u{1d546}" +
            "\u{1d54a}-\u{1d550}\u{1d552}-\u{1d6a5}\u{1d6a8}-\u{1d6c0}" +
            "\u{1d6c2}-\u{1d6da}\u{1d6dc}-\u{1d6fa}\u{1d6fc}-\u{1d714}" +
            "\u{1d716}-\u{1d734}\u{1d736}-\u{1d74e}\u{1d750}-\u{1d76e}" +
            "\u{1d770}-\u{1d788}\u{1d78a}-\u{1d7a8}\u{1d7aa}-\u{1d7c2}" +
            "\u{1d7c4}-\u{1d7cb}\u{1e100}-\u{1e12c}\u{1e137}-\u{1e13d}" +
            "\u{1e14e}\u{1e2c0}-\u{1e2eb}\u{1e800}-\u{1e8c4}" +
            "\u{1e900}-\u{1e943}\u{1e94b}\u{1ee00}-\u{1ee03}" +
            "\u{1ee05}-\u{1ee1f}\u{1ee21}\u{1ee22}\u{1ee24}\u{1ee27}" +
            "\u{1ee29}-\u{1ee32}\u{1ee34}-\u{1ee37}\u{1ee39}\u{1ee3b}" +
            "\u{1ee42}\u{1ee47}\u{1ee49}\u{1ee4b}\u{1ee4d}-\u{1ee4f}" +
            "\u{1ee51}\u{1ee52}\u{1ee54}\u{1ee57}\u{1ee59}\u{1ee5b}" +
            "\u{1ee5d}\u{1ee5f}\u{1ee61}\u{1ee62}\u{1ee64}" +
            "\u{1ee67}-\u{1ee6a}\u{1ee6c}-\u{1ee72}\u{1ee74}-\u{1ee77}" +
            "\u{1ee79}-\u{1ee7c}\u{1ee7e}\u{1ee80}-\u{1ee89}" +
            "\u{1ee8b}-\u{1ee9b}\u{1eea1}-\u{1eea3}\u{1eea5}-\u{1eea9}" +
            "\u{1eeab}-\u{1eebb}\u{20000}-\u{2a6d6}\u{2a700}-\u{2b734}" +
            "\u{2b740}-\u{2b81d}\u{2b820}-\u{2cea1}\u{2ceb0}-\u{2ebe0}" +
            "\u{2f800}-\u{2fa1d}]$", "u");
    }
    return largeIdStartPattern.test(String.fromCodePoint(cp));
}
function isLargeIdContinue(cp) {
    if (!largeIdContinuePattern) {
        largeIdContinuePattern = new RegExp("^[\xb7\u0300-\u036f\u0387\u0483-\u0487\u0591-\u05bd\u05bf" +
            "\u05c1\u05c2\u05c4\u05c5\u05c7\u0610-\u061a\u064b-\u0669" +
            "\u0670\u06d6-\u06dc\u06df-\u06e4\u06e7\u06e8\u06ea-\u06ed" +
            "\u06f0-\u06f9\u0711\u0730-\u074a\u07a6-\u07b0\u07c0-\u07c9" +
            "\u07eb-\u07f3\u07fd\u0816-\u0819\u081b-\u0823\u0825-\u0827" +
            "\u0829-\u082d\u0859-\u085b\u08d3-\u08e1\u08e3-\u0903" +
            "\u093a-\u093c\u093e-\u094f\u0951-\u0957\u0962\u0963" +
            "\u0966-\u096f\u0981-\u0983\u09bc\u09be-\u09c4\u09c7\u09c8" +
            "\u09cb-\u09cd\u09d7\u09e2\u09e3\u09e6-\u09ef\u09fe" +
            "\u0a01-\u0a03\u0a3c\u0a3e-\u0a42\u0a47\u0a48\u0a4b-\u0a4d" +
            "\u0a51\u0a66-\u0a71\u0a75\u0a81-\u0a83\u0abc\u0abe-\u0ac5" +
            "\u0ac7-\u0ac9\u0acb-\u0acd\u0ae2\u0ae3\u0ae6-\u0aef" +
            "\u0afa-\u0aff\u0b01-\u0b03\u0b3c\u0b3e-\u0b44\u0b47\u0b48" +
            "\u0b4b-\u0b4d\u0b56\u0b57\u0b62\u0b63\u0b66-\u0b6f\u0b82" +
            "\u0bbe-\u0bc2\u0bc6-\u0bc8\u0bca-\u0bcd\u0bd7\u0be6-\u0bef" +
            "\u0c00-\u0c04\u0c3e-\u0c44\u0c46-\u0c48\u0c4a-\u0c4d" +
            "\u0c55\u0c56\u0c62\u0c63\u0c66-\u0c6f\u0c81-\u0c83\u0cbc" +
            "\u0cbe-\u0cc4\u0cc6-\u0cc8\u0cca-\u0ccd\u0cd5\u0cd6" +
            "\u0ce2\u0ce3\u0ce6-\u0cef\u0d00-\u0d03\u0d3b\u0d3c" +
            "\u0d3e-\u0d44\u0d46-\u0d48\u0d4a-\u0d4d\u0d57\u0d62\u0d63" +
            "\u0d66-\u0d6f\u0d82\u0d83\u0dca\u0dcf-\u0dd4\u0dd6" +
            "\u0dd8-\u0ddf\u0de6-\u0def\u0df2\u0df3\u0e31\u0e34-\u0e3a" +
            "\u0e47-\u0e4e\u0e50-\u0e59\u0eb1\u0eb4-\u0ebc\u0ec8-\u0ecd" +
            "\u0ed0-\u0ed9\u0f18\u0f19\u0f20-\u0f29\u0f35\u0f37\u0f39" +
            "\u0f3e\u0f3f\u0f71-\u0f84\u0f86\u0f87\u0f8d-\u0f97" +
            "\u0f99-\u0fbc\u0fc6\u102b-\u103e\u1040-\u1049\u1056-\u1059" +
            "\u105e-\u1060\u1062-\u1064\u1067-\u106d\u1071-\u1074" +
            "\u1082-\u108d\u108f-\u109d\u135d-\u135f\u1369-\u1371" +
            "\u1712-\u1714\u1732-\u1734\u1752\u1753\u1772\u1773" +
            "\u17b4-\u17d3\u17dd\u17e0-\u17e9\u180b-\u180d\u1810-\u1819" +
            "\u18a9\u1920-\u192b\u1930-\u193b\u1946-\u194f\u19d0-\u19da" +
            "\u1a17-\u1a1b\u1a55-\u1a5e\u1a60-\u1a7c\u1a7f-\u1a89" +
            "\u1a90-\u1a99\u1ab0-\u1abd\u1b00-\u1b04\u1b34-\u1b44" +
            "\u1b50-\u1b59\u1b6b-\u1b73\u1b80-\u1b82\u1ba1-\u1bad" +
            "\u1bb0-\u1bb9\u1be6-\u1bf3\u1c24-\u1c37\u1c40-\u1c49" +
            "\u1c50-\u1c59\u1cd0-\u1cd2\u1cd4-\u1ce8\u1ced\u1cf4" +
            "\u1cf7-\u1cf9\u1dc0-\u1df9\u1dfb-\u1dff\u203f\u2040\u2054" +
            "\u20d0-\u20dc\u20e1\u20e5-\u20f0\u2cef-\u2cf1\u2d7f" +
            "\u2de0-\u2dff\u302a-\u302f\u3099\u309a\ua620-\ua629\ua66f" +
            "\ua674-\ua67d\ua69e\ua69f\ua6f0\ua6f1\ua802\ua806\ua80b" +
            "\ua823-\ua827\ua880\ua881\ua8b4-\ua8c5\ua8d0-\ua8d9" +
            "\ua8e0-\ua8f1\ua8ff-\ua909\ua926-\ua92d\ua947-\ua953" +
            "\ua980-\ua983\ua9b3-\ua9c0\ua9d0-\ua9d9\ua9e5\ua9f0-\ua9f9" +
            "\uaa29-\uaa36\uaa43\uaa4c\uaa4d\uaa50-\uaa59\uaa7b-\uaa7d" +
            "\uaab0\uaab2-\uaab4\uaab7\uaab8\uaabe\uaabf\uaac1" +
            "\uaaeb-\uaaef\uaaf5\uaaf6\uabe3-\uabea\uabec\uabed" +
            "\uabf0-\uabf9\ufb1e\ufe00-\ufe0f\ufe20-\ufe2f\ufe33\ufe34" +
            "\ufe4d-\ufe4f\uff10-\uff19\uff3f\u{101fd}\u{102e0}" +
            "\u{10376}-\u{1037a}\u{104a0}-\u{104a9}\u{10a01}-\u{10a03}" +
            "\u{10a05}\u{10a06}\u{10a0c}-\u{10a0f}\u{10a38}-\u{10a3a}" +
            "\u{10a3f}\u{10ae5}\u{10ae6}\u{10d24}-\u{10d27}" +
            "\u{10d30}-\u{10d39}\u{10f46}-\u{10f50}\u{11000}-\u{11002}" +
            "\u{11038}-\u{11046}\u{11066}-\u{1106f}\u{1107f}-\u{11082}" +
            "\u{110b0}-\u{110ba}\u{110f0}-\u{110f9}\u{11100}-\u{11102}" +
            "\u{11127}-\u{11134}\u{11136}-\u{1113f}\u{11145}\u{11146}" +
            "\u{11173}\u{11180}-\u{11182}\u{111b3}-\u{111c0}" +
            "\u{111c9}-\u{111cc}\u{111d0}-\u{111d9}\u{1122c}-\u{11237}" +
            "\u{1123e}\u{112df}-\u{112ea}\u{112f0}-\u{112f9}" +
            "\u{11300}-\u{11303}\u{1133b}\u{1133c}\u{1133e}-\u{11344}" +
            "\u{11347}\u{11348}\u{1134b}-\u{1134d}\u{11357}" +
            "\u{11362}\u{11363}\u{11366}-\u{1136c}\u{11370}-\u{11374}" +
            "\u{11435}-\u{11446}\u{11450}-\u{11459}\u{1145e}" +
            "\u{114b0}-\u{114c3}\u{114d0}-\u{114d9}\u{115af}-\u{115b5}" +
            "\u{115b8}-\u{115c0}\u{115dc}\u{115dd}\u{11630}-\u{11640}" +
            "\u{11650}-\u{11659}\u{116ab}-\u{116b7}\u{116c0}-\u{116c9}" +
            "\u{1171d}-\u{1172b}\u{11730}-\u{11739}\u{1182c}-\u{1183a}" +
            "\u{118e0}-\u{118e9}\u{119d1}-\u{119d7}\u{119da}-\u{119e0}" +
            "\u{119e4}\u{11a01}-\u{11a0a}\u{11a33}-\u{11a39}" +
            "\u{11a3b}-\u{11a3e}\u{11a47}\u{11a51}-\u{11a5b}" +
            "\u{11a8a}-\u{11a99}\u{11c2f}-\u{11c36}\u{11c38}-\u{11c3f}" +
            "\u{11c50}-\u{11c59}\u{11c92}-\u{11ca7}\u{11ca9}-\u{11cb6}" +
            "\u{11d31}-\u{11d36}\u{11d3a}\u{11d3c}\u{11d3d}" +
            "\u{11d3f}-\u{11d45}\u{11d47}\u{11d50}-\u{11d59}" +
            "\u{11d8a}-\u{11d8e}\u{11d90}\u{11d91}\u{11d93}-\u{11d97}" +
            "\u{11da0}-\u{11da9}\u{11ef3}-\u{11ef6}\u{16a60}-\u{16a69}" +
            "\u{16af0}-\u{16af4}\u{16b30}-\u{16b36}\u{16b50}-\u{16b59}" +
            "\u{16f4f}\u{16f51}-\u{16f87}\u{16f8f}-\u{16f92}" +
            "\u{1bc9d}\u{1bc9e}\u{1d165}-\u{1d169}\u{1d16d}-\u{1d172}" +
            "\u{1d17b}-\u{1d182}\u{1d185}-\u{1d18b}\u{1d1aa}-\u{1d1ad}" +
            "\u{1d242}-\u{1d244}\u{1d7ce}-\u{1d7ff}\u{1da00}-\u{1da36}" +
            "\u{1da3b}-\u{1da6c}\u{1da75}\u{1da84}\u{1da9b}-\u{1da9f}" +
            "\u{1daa1}-\u{1daaf}\u{1e000}-\u{1e006}\u{1e008}-\u{1e018}" +
            "\u{1e01b}-\u{1e021}\u{1e023}\u{1e024}\u{1e026}-\u{1e02a}" +
            "\u{1e130}-\u{1e136}\u{1e140}-\u{1e149}\u{1e2ec}-\u{1e2f9}" +
            "\u{1e8d0}-\u{1e8d6}\u{1e944}-\u{1e94a}\u{1e950}-\u{1e959}" +
            "\u{e0100}-\u{e01ef}]$", "u");
    }
    return largeIdContinuePattern.test(String.fromCodePoint(cp));
}

const gcNamePattern = /^(?:General_Category|gc)$/u;
const scNamePattern = /^(?:Script(?:_Extensions)?|scx?)$/u;
const gcValuePatterns = {
    es2018: null,
    es2019: null,
    es2020: null,
};
const scValuePatterns = {
    es2018: null,
    es2019: null,
    es2020: null,
};
const binPropertyPatterns = {
    es2018: null,
    es2019: null,
    es2020: null,
};
function isValidUnicodeProperty(version, name, value) {
    if (gcNamePattern.test(name)) {
        if (version >= 2018) {
            if (!gcValuePatterns.es2018) {
                gcValuePatterns.es2018 = new RegExp("^(?:C|Cased_Letter|Cc|Cf|Close_Punctuation|Cn|Co|" +
                    "Combining_Mark|Connector_Punctuation|Control|Cs|" +
                    "Currency_Symbol|Dash_Punctuation|Decimal_Number|" +
                    "Enclosing_Mark|Final_Punctuation|Format|" +
                    "Initial_Punctuation|L|LC|Letter|Letter_Number|" +
                    "Line_Separator|Ll|Lm|Lo|Lowercase_Letter|Lt|Lu|M|" +
                    "Mark|Math_Symbol|Mc|Me|Mn|Modifier_Letter|" +
                    "Modifier_Symbol|N|Nd|Nl|No|Nonspacing_Mark|Number|" +
                    "Open_Punctuation|Other|Other_Letter|Other_Number|" +
                    "Other_Punctuation|Other_Symbol|P|" +
                    "Paragraph_Separator|Pc|Pd|Pe|Pf|Pi|Po|Private_Use|" +
                    "Ps|Punctuation|S|Sc|Separator|Sk|Sm|So|" +
                    "Space_Separator|Spacing_Mark|Surrogate|Symbol|" +
                    "Titlecase_Letter|Unassigned|Uppercase_Letter|Z|Zl|" +
                    "Zp|Zs|cntrl|digit|punct)$", "u");
            }
            if (gcValuePatterns.es2018.test(value)) {
                return true;
            }
        }
    }
    if (scNamePattern.test(name)) {
        if (version >= 2018) {
            if (!scValuePatterns.es2018) {
                scValuePatterns.es2018 = new RegExp("^(?:Adlam|Adlm|Aghb|Ahom|Anatolian_Hieroglyphs|Arab|" +
                    "Arabic|Armenian|Armi|Armn|Avestan|Avst|Bali|" +
                    "Balinese|Bamu|Bamum|Bass|Bassa_Vah|Batak|Batk|Beng|" +
                    "Bengali|Bhaiksuki|Bhks|Bopo|Bopomofo|Brah|Brahmi|" +
                    "Brai|Braille|Bugi|Buginese|Buhd|Buhid|Cakm|" +
                    "Canadian_Aboriginal|Cans|Cari|Carian|" +
                    "Caucasian_Albanian|Chakma|Cham|Cher|Cherokee|Common|" +
                    "Copt|Coptic|Cprt|Cuneiform|Cypriot|Cyrillic|Cyrl|" +
                    "Deseret|Deva|Devanagari|Dsrt|Dupl|Duployan|Egyp|" +
                    "Egyptian_Hieroglyphs|Elba|Elbasan|Ethi|Ethiopic|" +
                    "Geor|Georgian|Glag|Glagolitic|Gonm|Goth|Gothic|Gran|" +
                    "Grantha|Greek|Grek|Gujarati|Gujr|Gurmukhi|Guru|Han|" +
                    "Hang|Hangul|Hani|Hano|Hanunoo|Hatr|Hatran|Hebr|" +
                    "Hebrew|Hira|Hiragana|Hluw|Hmng|Hung|" +
                    "Imperial_Aramaic|Inherited|Inscriptional_Pahlavi|" +
                    "Inscriptional_Parthian|Ital|Java|Javanese|Kaithi|" +
                    "Kali|Kana|Kannada|Katakana|Kayah_Li|Khar|Kharoshthi|" +
                    "Khmer|Khmr|Khoj|Khojki|Khudawadi|Knda|Kthi|Lana|Lao|" +
                    "Laoo|Latin|Latn|Lepc|Lepcha|Limb|Limbu|Lina|Linb|" +
                    "Linear_A|Linear_B|Lisu|Lyci|Lycian|Lydi|Lydian|" +
                    "Mahajani|Mahj|Malayalam|Mand|Mandaic|Mani|" +
                    "Manichaean|Marc|Marchen|Masaram_Gondi|Meetei_Mayek|" +
                    "Mend|Mende_Kikakui|Merc|Mero|Meroitic_Cursive|" +
                    "Meroitic_Hieroglyphs|Miao|Mlym|Modi|Mong|Mongolian|" +
                    "Mro|Mroo|Mtei|Mult|Multani|Myanmar|Mymr|Nabataean|" +
                    "Narb|Nbat|New_Tai_Lue|Newa|Nko|Nkoo|Nshu|Nushu|Ogam|" +
                    "Ogham|Ol_Chiki|Olck|Old_Hungarian|Old_Italic|" +
                    "Old_North_Arabian|Old_Permic|Old_Persian|" +
                    "Old_South_Arabian|Old_Turkic|Oriya|Orkh|Orya|Osage|" +
                    "Osge|Osma|Osmanya|Pahawh_Hmong|Palm|Palmyrene|" +
                    "Pau_Cin_Hau|Pauc|Perm|Phag|Phags_Pa|Phli|Phlp|Phnx|" +
                    "Phoenician|Plrd|Prti|Psalter_Pahlavi|Qaac|Qaai|" +
                    "Rejang|Rjng|Runic|Runr|Samaritan|Samr|Sarb|Saur|" +
                    "Saurashtra|Sgnw|Sharada|Shavian|Shaw|Shrd|Sidd|" +
                    "Siddham|SignWriting|Sind|Sinh|Sinhala|Sora|" +
                    "Sora_Sompeng|Soyo|Soyombo|Sund|Sundanese|Sylo|" +
                    "Syloti_Nagri|Syrc|Syriac|Tagalog|Tagb|Tagbanwa|" +
                    "Tai_Le|Tai_Tham|Tai_Viet|Takr|Takri|Tale|Talu|Tamil|" +
                    "Taml|Tang|Tangut|Tavt|Telu|Telugu|Tfng|Tglg|Thaa|" +
                    "Thaana|Thai|Tibetan|Tibt|Tifinagh|Tirh|Tirhuta|Ugar|" +
                    "Ugaritic|Vai|Vaii|Wara|Warang_Citi|Xpeo|Xsux|Yi|" +
                    "Yiii|Zanabazar_Square|Zanb|Zinh|Zyyy)$", "u");
            }
            if (scValuePatterns.es2018.test(value)) {
                return true;
            }
        }
        if (version >= 2019) {
            if (!scValuePatterns.es2019) {
                scValuePatterns.es2019 = new RegExp("^(?:Dogr|Dogra|Gong|Gunjala_Gondi|Hanifi_Rohingya|" +
                    "Maka|Makasar|Medefaidrin|Medf|Old_Sogdian|Rohg|Sogd|" +
                    "Sogdian|Sogo)$", "u");
            }
            if (scValuePatterns.es2019.test(value)) {
                return true;
            }
        }
        if (version >= 2020) {
            if (!scValuePatterns.es2020) {
                scValuePatterns.es2020 = new RegExp("^(?:Hmnp|Nand|Nandinagari|Nyiakeng_Puachue_Hmong|" +
                    "Wancho|Wcho)$", "u");
            }
            if (scValuePatterns.es2020.test(value)) {
                return true;
            }
        }
    }
    return false;
}
function isValidLoneUnicodeProperty(version, value) {
    if (version >= 2018) {
        if (!binPropertyPatterns.es2018) {
            binPropertyPatterns.es2018 = new RegExp("^(?:AHex|ASCII|ASCII_Hex_Digit|Alpha|Alphabetic|Any|" +
                "Assigned|Bidi_C|Bidi_Control|Bidi_M|Bidi_Mirrored|CI|" +
                "CWCF|CWCM|CWKCF|CWL|CWT|CWU|Case_Ignorable|Cased|" +
                "Changes_When_Casefolded|Changes_When_Casemapped|" +
                "Changes_When_Lowercased|Changes_When_NFKC_Casefolded|" +
                "Changes_When_Titlecased|Changes_When_Uppercased|DI|Dash|" +
                "Default_Ignorable_Code_Point|Dep|Deprecated|Dia|" +
                "Diacritic|Emoji|Emoji_Component|Emoji_Modifier|" +
                "Emoji_Modifier_Base|Emoji_Presentation|Ext|Extender|" +
                "Gr_Base|Gr_Ext|Grapheme_Base|Grapheme_Extend|Hex|" +
                "Hex_Digit|IDC|IDS|IDSB|IDST|IDS_Binary_Operator|" +
                "IDS_Trinary_Operator|ID_Continue|ID_Start|Ideo|" +
                "Ideographic|Join_C|Join_Control|LOE|" +
                "Logical_Order_Exception|Lower|Lowercase|Math|NChar|" +
                "Noncharacter_Code_Point|Pat_Syn|Pat_WS|Pattern_Syntax|" +
                "Pattern_White_Space|QMark|Quotation_Mark|RI|Radical|" +
                "Regional_Indicator|SD|STerm|Sentence_Terminal|" +
                "Soft_Dotted|Term|Terminal_Punctuation|UIdeo|" +
                "Unified_Ideograph|Upper|Uppercase|VS|Variation_Selector|" +
                "White_Space|XIDC|XIDS|XID_Continue|XID_Start|space)$", "u");
        }
        if (binPropertyPatterns.es2018.test(value)) {
            return true;
        }
    }
    if (version >= 2019) {
        if (!binPropertyPatterns.es2019) {
            binPropertyPatterns.es2019 = new RegExp("^(?:Extended_Pictographic)$", "u");
        }
        if (binPropertyPatterns.es2019.test(value)) {
            return true;
        }
    }
    return false;
}

const Backspace = 0x08;
const CharacterTabulation = 0x09;
const LineFeed = 0x0a;
const LineTabulation = 0x0b;
const FormFeed = 0x0c;
const CarriageReturn = 0x0d;
const ExclamationMark = 0x21;
const DollarSign = 0x24;
const LeftParenthesis = 0x28;
const RightParenthesis = 0x29;
const Asterisk = 0x2a;
const PlusSign = 0x2b;
const Comma = 0x2c;
const HyphenMinus = 0x2d;
const FullStop = 0x2e;
const Solidus = 0x2f;
const DigitZero = 0x30;
const DigitOne = 0x31;
const DigitSeven = 0x37;
const DigitNine = 0x39;
const Colon = 0x3a;
const LessThanSign = 0x3c;
const EqualsSign = 0x3d;
const GreaterThanSign = 0x3e;
const QuestionMark = 0x3f;
const LatinCapitalLetterA = 0x41;
const LatinCapitalLetterB = 0x42;
const LatinCapitalLetterD = 0x44;
const LatinCapitalLetterF = 0x46;
const LatinCapitalLetterP = 0x50;
const LatinCapitalLetterS = 0x53;
const LatinCapitalLetterW = 0x57;
const LatinCapitalLetterZ = 0x5a;
const LowLine = 0x5f;
const LatinSmallLetterA = 0x61;
const LatinSmallLetterB = 0x62;
const LatinSmallLetterC = 0x63;
const LatinSmallLetterD = 0x64;
const LatinSmallLetterF = 0x66;
const LatinSmallLetterG = 0x67;
const LatinSmallLetterI = 0x69;
const LatinSmallLetterK = 0x6b;
const LatinSmallLetterM = 0x6d;
const LatinSmallLetterN = 0x6e;
const LatinSmallLetterP = 0x70;
const LatinSmallLetterR = 0x72;
const LatinSmallLetterS = 0x73;
const LatinSmallLetterT = 0x74;
const LatinSmallLetterU = 0x75;
const LatinSmallLetterV = 0x76;
const LatinSmallLetterW = 0x77;
const LatinSmallLetterX = 0x78;
const LatinSmallLetterY = 0x79;
const LatinSmallLetterZ = 0x7a;
const LeftSquareBracket = 0x5b;
const ReverseSolidus = 0x5c;
const RightSquareBracket = 0x5d;
const CircumflexAccent = 0x5e;
const LeftCurlyBracket = 0x7b;
const VerticalLine = 0x7c;
const RightCurlyBracket = 0x7d;
const ZeroWidthNonJoiner = 0x200c;
const ZeroWidthJoiner = 0x200d;
const LineSeparator = 0x2028;
const ParagraphSeparator = 0x2029;
const MinCodePoint = 0x00;
const MaxCodePoint = 0x10ffff;
function isLatinLetter(code) {
    return ((code >= LatinCapitalLetterA && code <= LatinCapitalLetterZ) ||
        (code >= LatinSmallLetterA && code <= LatinSmallLetterZ));
}
function isDecimalDigit(code) {
    return code >= DigitZero && code <= DigitNine;
}
function isOctalDigit(code) {
    return code >= DigitZero && code <= DigitSeven;
}
function isHexDigit(code) {
    return ((code >= DigitZero && code <= DigitNine) ||
        (code >= LatinCapitalLetterA && code <= LatinCapitalLetterF) ||
        (code >= LatinSmallLetterA && code <= LatinSmallLetterF));
}
function isLineTerminator(code) {
    return (code === LineFeed ||
        code === CarriageReturn ||
        code === LineSeparator ||
        code === ParagraphSeparator);
}
function isValidUnicode(code) {
    return code >= MinCodePoint && code <= MaxCodePoint;
}
function digitToInt(code) {
    if (code >= LatinSmallLetterA && code <= LatinSmallLetterF) {
        return code - LatinSmallLetterA + 10;
    }
    if (code >= LatinCapitalLetterA && code <= LatinCapitalLetterF) {
        return code - LatinCapitalLetterA + 10;
    }
    return code - DigitZero;
}

const legacyImpl = {
    at(s, end, i) {
        return i < end ? s.charCodeAt(i) : -1;
    },
    width(c) {
        return 1;
    },
};
const unicodeImpl = {
    at(s, end, i) {
        return i < end ? s.codePointAt(i) : -1;
    },
    width(c) {
        return c > 0xffff ? 2 : 1;
    },
};
class Reader {
    constructor() {
        this._impl = legacyImpl;
        this._s = "";
        this._i = 0;
        this._end = 0;
        this._cp1 = -1;
        this._w1 = 1;
        this._cp2 = -1;
        this._w2 = 1;
        this._cp3 = -1;
        this._w3 = 1;
        this._cp4 = -1;
    }
    get source() {
        return this._s;
    }
    get index() {
        return this._i;
    }
    get currentCodePoint() {
        return this._cp1;
    }
    get nextCodePoint() {
        return this._cp2;
    }
    get nextCodePoint2() {
        return this._cp3;
    }
    get nextCodePoint3() {
        return this._cp4;
    }
    reset(source, start, end, uFlag) {
        this._impl = uFlag ? unicodeImpl : legacyImpl;
        this._s = source;
        this._end = end;
        this.rewind(start);
    }
    rewind(index) {
        const impl = this._impl;
        this._i = index;
        this._cp1 = impl.at(this._s, this._end, index);
        this._w1 = impl.width(this._cp1);
        this._cp2 = impl.at(this._s, this._end, index + this._w1);
        this._w2 = impl.width(this._cp2);
        this._cp3 = impl.at(this._s, this._end, index + this._w1 + this._w2);
        this._w3 = impl.width(this._cp3);
        this._cp4 = impl.at(this._s, this._end, index + this._w1 + this._w2 + this._w3);
    }
    advance() {
        if (this._cp1 !== -1) {
            const impl = this._impl;
            this._i += this._w1;
            this._cp1 = this._cp2;
            this._w1 = this._w2;
            this._cp2 = this._cp3;
            this._w2 = impl.width(this._cp2);
            this._cp3 = this._cp4;
            this._w3 = impl.width(this._cp3);
            this._cp4 = impl.at(this._s, this._end, this._i + this._w1 + this._w2 + this._w3);
        }
    }
    eat(cp) {
        if (this._cp1 === cp) {
            this.advance();
            return true;
        }
        return false;
    }
    eat2(cp1, cp2) {
        if (this._cp1 === cp1 && this._cp2 === cp2) {
            this.advance();
            this.advance();
            return true;
        }
        return false;
    }
    eat3(cp1, cp2, cp3) {
        if (this._cp1 === cp1 && this._cp2 === cp2 && this._cp3 === cp3) {
            this.advance();
            this.advance();
            this.advance();
            return true;
        }
        return false;
    }
}

class RegExpSyntaxError extends SyntaxError {
    constructor(source, uFlag, index, message) {
        if (source) {
            if (!source.startsWith("/")) {
                source = `/${source}/${uFlag ? "u" : ""}`;
            }
            source = `: ${source}`;
        }
        super(`Invalid regular expression${source}: ${message}`);
        this.index = index;
    }
}

function isSyntaxCharacter(cp) {
    return (cp === CircumflexAccent ||
        cp === DollarSign ||
        cp === ReverseSolidus ||
        cp === FullStop ||
        cp === Asterisk ||
        cp === PlusSign ||
        cp === QuestionMark ||
        cp === LeftParenthesis ||
        cp === RightParenthesis ||
        cp === LeftSquareBracket ||
        cp === RightSquareBracket ||
        cp === LeftCurlyBracket ||
        cp === RightCurlyBracket ||
        cp === VerticalLine);
}
function isRegExpIdentifierStart(cp) {
    return isIdStart(cp) || cp === DollarSign || cp === LowLine;
}
function isRegExpIdentifierPart(cp) {
    return (isIdContinue(cp) ||
        cp === DollarSign ||
        cp === LowLine ||
        cp === ZeroWidthNonJoiner ||
        cp === ZeroWidthJoiner);
}
function isUnicodePropertyNameCharacter(cp) {
    return isLatinLetter(cp) || cp === LowLine;
}
function isUnicodePropertyValueCharacter(cp) {
    return isUnicodePropertyNameCharacter(cp) || isDecimalDigit(cp);
}
class RegExpValidator {
    constructor(options) {
        this._reader = new Reader();
        this._uFlag = false;
        this._nFlag = false;
        this._lastIntValue = 0;
        this._lastMinValue = 0;
        this._lastMaxValue = 0;
        this._lastStrValue = "";
        this._lastKeyValue = "";
        this._lastValValue = "";
        this._lastAssertionIsQuantifiable = false;
        this._numCapturingParens = 0;
        this._groupNames = new Set();
        this._backreferenceNames = new Set();
        this._options = options || {};
    }
    validateLiteral(source, start = 0, end = source.length) {
        this._uFlag = this._nFlag = false;
        this.reset(source, start, end);
        this.onLiteralEnter(start);
        if (this.eat(Solidus) && this.eatRegExpBody() && this.eat(Solidus)) {
            const flagStart = this.index;
            const uFlag = source.includes("u", flagStart);
            this.validateFlags(source, flagStart, end);
            this.validatePattern(source, start + 1, flagStart - 1, uFlag);
        }
        else if (start >= end) {
            this.raise("Empty");
        }
        else {
            const c = String.fromCodePoint(this.currentCodePoint);
            this.raise(`Unexpected character '${c}'`);
        }
        this.onLiteralLeave(start, end);
    }
    validateFlags(source, start = 0, end = source.length) {
        const existingFlags = new Set();
        let global = false;
        let ignoreCase = false;
        let multiline = false;
        let sticky = false;
        let unicode = false;
        let dotAll = false;
        for (let i = start; i < end; ++i) {
            const flag = source.charCodeAt(i);
            if (existingFlags.has(flag)) {
                this.raise(`Duplicated flag '${source[i]}'`);
            }
            existingFlags.add(flag);
            if (flag === LatinSmallLetterG) {
                global = true;
            }
            else if (flag === LatinSmallLetterI) {
                ignoreCase = true;
            }
            else if (flag === LatinSmallLetterM) {
                multiline = true;
            }
            else if (flag === LatinSmallLetterU && this.ecmaVersion >= 2015) {
                unicode = true;
            }
            else if (flag === LatinSmallLetterY && this.ecmaVersion >= 2015) {
                sticky = true;
            }
            else if (flag === LatinSmallLetterS && this.ecmaVersion >= 2018) {
                dotAll = true;
            }
            else {
                this.raise(`Invalid flag '${source[i]}'`);
            }
        }
        this.onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll);
    }
    validatePattern(source, start = 0, end = source.length, uFlag = false) {
        this._uFlag = uFlag && this.ecmaVersion >= 2015;
        this._nFlag = uFlag && this.ecmaVersion >= 2018;
        this.reset(source, start, end);
        this.pattern();
        if (!this._nFlag &&
            this.ecmaVersion >= 2018 &&
            this._groupNames.size > 0) {
            this._nFlag = true;
            this.rewind(start);
            this.pattern();
        }
    }
    get strict() {
        return Boolean(this._options.strict || this._uFlag);
    }
    get ecmaVersion() {
        return this._options.ecmaVersion || 2020;
    }
    onLiteralEnter(start) {
        if (this._options.onLiteralEnter) {
            this._options.onLiteralEnter(start);
        }
    }
    onLiteralLeave(start, end) {
        if (this._options.onLiteralLeave) {
            this._options.onLiteralLeave(start, end);
        }
    }
    onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll) {
        if (this._options.onFlags) {
            this._options.onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll);
        }
    }
    onPatternEnter(start) {
        if (this._options.onPatternEnter) {
            this._options.onPatternEnter(start);
        }
    }
    onPatternLeave(start, end) {
        if (this._options.onPatternLeave) {
            this._options.onPatternLeave(start, end);
        }
    }
    onDisjunctionEnter(start) {
        if (this._options.onDisjunctionEnter) {
            this._options.onDisjunctionEnter(start);
        }
    }
    onDisjunctionLeave(start, end) {
        if (this._options.onDisjunctionLeave) {
            this._options.onDisjunctionLeave(start, end);
        }
    }
    onAlternativeEnter(start, index) {
        if (this._options.onAlternativeEnter) {
            this._options.onAlternativeEnter(start, index);
        }
    }
    onAlternativeLeave(start, end, index) {
        if (this._options.onAlternativeLeave) {
            this._options.onAlternativeLeave(start, end, index);
        }
    }
    onGroupEnter(start) {
        if (this._options.onGroupEnter) {
            this._options.onGroupEnter(start);
        }
    }
    onGroupLeave(start, end) {
        if (this._options.onGroupLeave) {
            this._options.onGroupLeave(start, end);
        }
    }
    onCapturingGroupEnter(start, name) {
        if (this._options.onCapturingGroupEnter) {
            this._options.onCapturingGroupEnter(start, name);
        }
    }
    onCapturingGroupLeave(start, end, name) {
        if (this._options.onCapturingGroupLeave) {
            this._options.onCapturingGroupLeave(start, end, name);
        }
    }
    onQuantifier(start, end, min, max, greedy) {
        if (this._options.onQuantifier) {
            this._options.onQuantifier(start, end, min, max, greedy);
        }
    }
    onLookaroundAssertionEnter(start, kind, negate) {
        if (this._options.onLookaroundAssertionEnter) {
            this._options.onLookaroundAssertionEnter(start, kind, negate);
        }
    }
    onLookaroundAssertionLeave(start, end, kind, negate) {
        if (this._options.onLookaroundAssertionLeave) {
            this._options.onLookaroundAssertionLeave(start, end, kind, negate);
        }
    }
    onEdgeAssertion(start, end, kind) {
        if (this._options.onEdgeAssertion) {
            this._options.onEdgeAssertion(start, end, kind);
        }
    }
    onWordBoundaryAssertion(start, end, kind, negate) {
        if (this._options.onWordBoundaryAssertion) {
            this._options.onWordBoundaryAssertion(start, end, kind, negate);
        }
    }
    onAnyCharacterSet(start, end, kind) {
        if (this._options.onAnyCharacterSet) {
            this._options.onAnyCharacterSet(start, end, kind);
        }
    }
    onEscapeCharacterSet(start, end, kind, negate) {
        if (this._options.onEscapeCharacterSet) {
            this._options.onEscapeCharacterSet(start, end, kind, negate);
        }
    }
    onUnicodePropertyCharacterSet(start, end, kind, key, value, negate) {
        if (this._options.onUnicodePropertyCharacterSet) {
            this._options.onUnicodePropertyCharacterSet(start, end, kind, key, value, negate);
        }
    }
    onCharacter(start, end, value) {
        if (this._options.onCharacter) {
            this._options.onCharacter(start, end, value);
        }
    }
    onBackreference(start, end, ref) {
        if (this._options.onBackreference) {
            this._options.onBackreference(start, end, ref);
        }
    }
    onCharacterClassEnter(start, negate) {
        if (this._options.onCharacterClassEnter) {
            this._options.onCharacterClassEnter(start, negate);
        }
    }
    onCharacterClassLeave(start, end, negate) {
        if (this._options.onCharacterClassLeave) {
            this._options.onCharacterClassLeave(start, end, negate);
        }
    }
    onCharacterClassRange(start, end, min, max) {
        if (this._options.onCharacterClassRange) {
            this._options.onCharacterClassRange(start, end, min, max);
        }
    }
    get source() {
        return this._reader.source;
    }
    get index() {
        return this._reader.index;
    }
    get currentCodePoint() {
        return this._reader.currentCodePoint;
    }
    get nextCodePoint() {
        return this._reader.nextCodePoint;
    }
    get nextCodePoint2() {
        return this._reader.nextCodePoint2;
    }
    get nextCodePoint3() {
        return this._reader.nextCodePoint3;
    }
    reset(source, start, end) {
        this._reader.reset(source, start, end, this._uFlag);
    }
    rewind(index) {
        this._reader.rewind(index);
    }
    advance() {
        this._reader.advance();
    }
    eat(cp) {
        return this._reader.eat(cp);
    }
    eat2(cp1, cp2) {
        return this._reader.eat2(cp1, cp2);
    }
    eat3(cp1, cp2, cp3) {
        return this._reader.eat3(cp1, cp2, cp3);
    }
    raise(message) {
        throw new RegExpSyntaxError(this.source, this._uFlag, this.index, message);
    }
    eatRegExpBody() {
        const start = this.index;
        let inClass = false;
        let escaped = false;
        for (;;) {
            const cp = this.currentCodePoint;
            if (cp === -1 || isLineTerminator(cp)) {
                const kind = inClass ? "character class" : "regular expression";
                this.raise(`Unterminated ${kind}`);
            }
            if (escaped) {
                escaped = false;
            }
            else if (cp === ReverseSolidus) {
                escaped = true;
            }
            else if (cp === LeftSquareBracket) {
                inClass = true;
            }
            else if (cp === RightSquareBracket) {
                inClass = false;
            }
            else if ((cp === Solidus && !inClass) ||
                (cp === Asterisk && this.index === start)) {
                break;
            }
            this.advance();
        }
        return this.index !== start;
    }
    pattern() {
        const start = this.index;
        this._numCapturingParens = this.countCapturingParens();
        this._groupNames.clear();
        this._backreferenceNames.clear();
        this.onPatternEnter(start);
        this.disjunction();
        const cp = this.currentCodePoint;
        if (this.currentCodePoint !== -1) {
            if (cp === RightParenthesis) {
                this.raise("Unmatched ')'");
            }
            if (cp === ReverseSolidus) {
                this.raise("\\ at end of pattern");
            }
            if (cp === RightSquareBracket || cp === RightCurlyBracket) {
                this.raise("Lone quantifier brackets");
            }
            const c = String.fromCodePoint(cp);
            this.raise(`Unexpected character '${c}'`);
        }
        for (const name of this._backreferenceNames) {
            if (!this._groupNames.has(name)) {
                this.raise("Invalid named capture referenced");
            }
        }
        this.onPatternLeave(start, this.index);
    }
    countCapturingParens() {
        const start = this.index;
        let inClass = false;
        let escaped = false;
        let count = 0;
        let cp = 0;
        while ((cp = this.currentCodePoint) !== -1) {
            if (escaped) {
                escaped = false;
            }
            else if (cp === ReverseSolidus) {
                escaped = true;
            }
            else if (cp === LeftSquareBracket) {
                inClass = true;
            }
            else if (cp === RightSquareBracket) {
                inClass = false;
            }
            else if (cp === LeftParenthesis &&
                !inClass &&
                (this.nextCodePoint !== QuestionMark ||
                    (this.nextCodePoint2 === LessThanSign &&
                        this.nextCodePoint3 !== EqualsSign &&
                        this.nextCodePoint3 !== ExclamationMark))) {
                count += 1;
            }
            this.advance();
        }
        this.rewind(start);
        return count;
    }
    disjunction() {
        const start = this.index;
        let i = 0;
        this.onDisjunctionEnter(start);
        this.alternative(i++);
        while (this.eat(VerticalLine)) {
            this.alternative(i++);
        }
        if (this.eatQuantifier(true)) {
            this.raise("Nothing to repeat");
        }
        if (this.eat(LeftCurlyBracket)) {
            this.raise("Lone quantifier brackets");
        }
        this.onDisjunctionLeave(start, this.index);
    }
    alternative(i) {
        const start = this.index;
        this.onAlternativeEnter(start, i);
        while (this.currentCodePoint !== -1 && this.eatTerm()) {
        }
        this.onAlternativeLeave(start, this.index, i);
    }
    eatTerm() {
        if (this.eatAssertion()) {
            if (this._lastAssertionIsQuantifiable) {
                this.eatQuantifier();
            }
            return true;
        }
        if (this.strict ? this.eatAtom() : this.eatExtendedAtom()) {
            this.eatQuantifier();
            return true;
        }
        return false;
    }
    eatAssertion() {
        const start = this.index;
        this._lastAssertionIsQuantifiable = false;
        if (this.eat(CircumflexAccent)) {
            this.onEdgeAssertion(start, this.index, "start");
            return true;
        }
        if (this.eat(DollarSign)) {
            this.onEdgeAssertion(start, this.index, "end");
            return true;
        }
        if (this.eat2(ReverseSolidus, LatinCapitalLetterB)) {
            this.onWordBoundaryAssertion(start, this.index, "word", true);
            return true;
        }
        if (this.eat2(ReverseSolidus, LatinSmallLetterB)) {
            this.onWordBoundaryAssertion(start, this.index, "word", false);
            return true;
        }
        if (this.eat2(LeftParenthesis, QuestionMark)) {
            const lookbehind = this.ecmaVersion >= 2018 && this.eat(LessThanSign);
            let negate = false;
            if (this.eat(EqualsSign) || (negate = this.eat(ExclamationMark))) {
                const kind = lookbehind ? "lookbehind" : "lookahead";
                this.onLookaroundAssertionEnter(start, kind, negate);
                this.disjunction();
                if (!this.eat(RightParenthesis)) {
                    this.raise("Unterminated group");
                }
                this._lastAssertionIsQuantifiable = !lookbehind && !this.strict;
                this.onLookaroundAssertionLeave(start, this.index, kind, negate);
                return true;
            }
            this.rewind(start);
        }
        return false;
    }
    eatQuantifier(noError = false) {
        const start = this.index;
        let min = 0;
        let max = 0;
        let greedy = false;
        if (this.eat(Asterisk)) {
            min = 0;
            max = Number.POSITIVE_INFINITY;
        }
        else if (this.eat(PlusSign)) {
            min = 1;
            max = Number.POSITIVE_INFINITY;
        }
        else if (this.eat(QuestionMark)) {
            min = 0;
            max = 1;
        }
        else if (this.eatBracedQuantifier(noError)) {
            min = this._lastMinValue;
            max = this._lastMaxValue;
        }
        else {
            return false;
        }
        greedy = !this.eat(QuestionMark);
        if (!noError) {
            this.onQuantifier(start, this.index, min, max, greedy);
        }
        return true;
    }
    eatBracedQuantifier(noError) {
        const start = this.index;
        if (this.eat(LeftCurlyBracket)) {
            this._lastMinValue = 0;
            this._lastMaxValue = Number.POSITIVE_INFINITY;
            if (this.eatDecimalDigits()) {
                this._lastMinValue = this._lastMaxValue = this._lastIntValue;
                if (this.eat(Comma)) {
                    this._lastMaxValue = this.eatDecimalDigits()
                        ? this._lastIntValue
                        : Number.POSITIVE_INFINITY;
                }
                if (this.eat(RightCurlyBracket)) {
                    if (!noError && this._lastMaxValue < this._lastMinValue) {
                        this.raise("numbers out of order in {} quantifier");
                    }
                    return true;
                }
            }
            if (!noError && this.strict) {
                this.raise("Incomplete quantifier");
            }
            this.rewind(start);
        }
        return false;
    }
    eatAtom() {
        return (this.eatPatternCharacter() ||
            this.eatDot() ||
            this.eatReverseSolidusAtomEscape() ||
            this.eatCharacterClass() ||
            this.eatUncapturingGroup() ||
            this.eatCapturingGroup());
    }
    eatDot() {
        if (this.eat(FullStop)) {
            this.onAnyCharacterSet(this.index - 1, this.index, "any");
            return true;
        }
        return false;
    }
    eatReverseSolidusAtomEscape() {
        const start = this.index;
        if (this.eat(ReverseSolidus)) {
            if (this.eatAtomEscape()) {
                return true;
            }
            this.rewind(start);
        }
        return false;
    }
    eatUncapturingGroup() {
        const start = this.index;
        if (this.eat3(LeftParenthesis, QuestionMark, Colon)) {
            this.onGroupEnter(start);
            this.disjunction();
            if (!this.eat(RightParenthesis)) {
                this.raise("Unterminated group");
            }
            this.onGroupLeave(start, this.index);
            return true;
        }
        return false;
    }
    eatCapturingGroup() {
        const start = this.index;
        if (this.eat(LeftParenthesis)) {
            this._lastStrValue = "";
            if (this.ecmaVersion >= 2018) {
                this.groupSpecifier();
            }
            else if (this.currentCodePoint === QuestionMark) {
                this.raise("Invalid group");
            }
            const name = this._lastStrValue || null;
            this.onCapturingGroupEnter(start, name);
            this.disjunction();
            if (!this.eat(RightParenthesis)) {
                this.raise("Unterminated group");
            }
            this.onCapturingGroupLeave(start, this.index, name);
            return true;
        }
        return false;
    }
    eatExtendedAtom() {
        return (this.eatDot() ||
            this.eatReverseSolidusAtomEscape() ||
            this.eatReverseSolidusFollowedByC() ||
            this.eatCharacterClass() ||
            this.eatUncapturingGroup() ||
            this.eatCapturingGroup() ||
            this.eatInvalidBracedQuantifier() ||
            this.eatExtendedPatternCharacter());
    }
    eatReverseSolidusFollowedByC() {
        if (this.currentCodePoint === ReverseSolidus &&
            this.nextCodePoint === LatinSmallLetterC) {
            this._lastIntValue = this.currentCodePoint;
            this.advance();
            this.onCharacter(this.index - 1, this.index, ReverseSolidus);
            return true;
        }
        return false;
    }
    eatInvalidBracedQuantifier() {
        if (this.eatBracedQuantifier(true)) {
            this.raise("Nothing to repeat");
        }
        return false;
    }
    eatSyntaxCharacter() {
        if (isSyntaxCharacter(this.currentCodePoint)) {
            this._lastIntValue = this.currentCodePoint;
            this.advance();
            return true;
        }
        return false;
    }
    eatPatternCharacter() {
        const start = this.index;
        const cp = this.currentCodePoint;
        if (cp !== -1 && !isSyntaxCharacter(cp)) {
            this.advance();
            this.onCharacter(start, this.index, cp);
            return true;
        }
        return false;
    }
    eatExtendedPatternCharacter() {
        const start = this.index;
        const cp = this.currentCodePoint;
        if (cp !== -1 &&
            cp !== CircumflexAccent &&
            cp !== DollarSign &&
            cp !== ReverseSolidus &&
            cp !== FullStop &&
            cp !== Asterisk &&
            cp !== PlusSign &&
            cp !== QuestionMark &&
            cp !== LeftParenthesis &&
            cp !== RightParenthesis &&
            cp !== LeftSquareBracket &&
            cp !== VerticalLine) {
            this.advance();
            this.onCharacter(start, this.index, cp);
            return true;
        }
        return false;
    }
    groupSpecifier() {
        this._lastStrValue = "";
        if (this.eat(QuestionMark)) {
            if (this.eatGroupName()) {
                if (!this._groupNames.has(this._lastStrValue)) {
                    this._groupNames.add(this._lastStrValue);
                    return;
                }
                this.raise("Duplicate capture group name");
            }
            this.raise("Invalid group");
        }
    }
    eatGroupName() {
        this._lastStrValue = "";
        if (this.eat(LessThanSign)) {
            if (this.eatRegExpIdentifierName() && this.eat(GreaterThanSign)) {
                return true;
            }
            this.raise("Invalid capture group name");
        }
        return false;
    }
    eatRegExpIdentifierName() {
        this._lastStrValue = "";
        if (this.eatRegExpIdentifierStart()) {
            this._lastStrValue += String.fromCodePoint(this._lastIntValue);
            while (this.eatRegExpIdentifierPart()) {
                this._lastStrValue += String.fromCodePoint(this._lastIntValue);
            }
            return true;
        }
        return false;
    }
    eatRegExpIdentifierStart() {
        const start = this.index;
        let cp = this.currentCodePoint;
        this.advance();
        if (cp === ReverseSolidus && this.eatRegExpUnicodeEscapeSequence()) {
            cp = this._lastIntValue;
        }
        if (isRegExpIdentifierStart(cp)) {
            this._lastIntValue = cp;
            return true;
        }
        if (this.index !== start) {
            this.rewind(start);
        }
        return false;
    }
    eatRegExpIdentifierPart() {
        const start = this.index;
        let cp = this.currentCodePoint;
        this.advance();
        if (cp === ReverseSolidus && this.eatRegExpUnicodeEscapeSequence()) {
            cp = this._lastIntValue;
        }
        if (isRegExpIdentifierPart(cp)) {
            this._lastIntValue = cp;
            return true;
        }
        if (this.index !== start) {
            this.rewind(start);
        }
        return false;
    }
    eatAtomEscape() {
        if (this.eatBackreference() ||
            this.eatCharacterClassEscape() ||
            this.eatCharacterEscape() ||
            (this._nFlag && this.eatKGroupName())) {
            return true;
        }
        if (this.strict || this._uFlag) {
            this.raise("Invalid escape");
        }
        return false;
    }
    eatBackreference() {
        const start = this.index;
        if (this.eatDecimalEscape()) {
            const n = this._lastIntValue;
            if (n <= this._numCapturingParens) {
                this.onBackreference(start - 1, this.index, n);
                return true;
            }
            if (this.strict) {
                this.raise("Invalid escape");
            }
            this.rewind(start);
        }
        return false;
    }
    eatKGroupName() {
        const start = this.index;
        if (this.eat(LatinSmallLetterK)) {
            if (this.eatGroupName()) {
                const groupName = this._lastStrValue;
                this._backreferenceNames.add(groupName);
                this.onBackreference(start - 1, this.index, groupName);
                return true;
            }
            this.raise("Invalid named reference");
        }
        return false;
    }
    eatCharacterEscape() {
        const start = this.index;
        if (this.eatControlEscape() ||
            this.eatCControlLetter() ||
            this.eatZero() ||
            this.eatHexEscapeSequence() ||
            this.eatRegExpUnicodeEscapeSequence() ||
            (!this.strict && this.eatLegacyOctalEscapeSequence()) ||
            this.eatIdentityEscape()) {
            this.onCharacter(start - 1, this.index, this._lastIntValue);
            return true;
        }
        return false;
    }
    eatCControlLetter() {
        const start = this.index;
        if (this.eat(LatinSmallLetterC)) {
            if (this.eatControlLetter()) {
                return true;
            }
            this.rewind(start);
        }
        return false;
    }
    eatZero() {
        if (this.currentCodePoint === DigitZero &&
            !isDecimalDigit(this.nextCodePoint)) {
            this._lastIntValue = 0;
            this.advance();
            return true;
        }
        return false;
    }
    eatControlEscape() {
        if (this.eat(LatinSmallLetterT)) {
            this._lastIntValue = CharacterTabulation;
            return true;
        }
        if (this.eat(LatinSmallLetterN)) {
            this._lastIntValue = LineFeed;
            return true;
        }
        if (this.eat(LatinSmallLetterV)) {
            this._lastIntValue = LineTabulation;
            return true;
        }
        if (this.eat(LatinSmallLetterF)) {
            this._lastIntValue = FormFeed;
            return true;
        }
        if (this.eat(LatinSmallLetterR)) {
            this._lastIntValue = CarriageReturn;
            return true;
        }
        return false;
    }
    eatControlLetter() {
        const cp = this.currentCodePoint;
        if (isLatinLetter(cp)) {
            this.advance();
            this._lastIntValue = cp % 0x20;
            return true;
        }
        return false;
    }
    eatRegExpUnicodeEscapeSequence() {
        const start = this.index;
        if (this.eat(LatinSmallLetterU)) {
            if (this.eatFixedHexDigits(4)) {
                const lead = this._lastIntValue;
                if (this._uFlag && lead >= 0xd800 && lead <= 0xdbff) {
                    const leadSurrogateEnd = this.index;
                    if (this.eat(ReverseSolidus) &&
                        this.eat(LatinSmallLetterU) &&
                        this.eatFixedHexDigits(4)) {
                        const trail = this._lastIntValue;
                        if (trail >= 0xdc00 && trail <= 0xdfff) {
                            this._lastIntValue =
                                (lead - 0xd800) * 0x400 +
                                    (trail - 0xdc00) +
                                    0x10000;
                            return true;
                        }
                    }
                    this.rewind(leadSurrogateEnd);
                    this._lastIntValue = lead;
                }
                return true;
            }
            if (this._uFlag &&
                this.eat(LeftCurlyBracket) &&
                this.eatHexDigits() &&
                this.eat(RightCurlyBracket) &&
                isValidUnicode(this._lastIntValue)) {
                return true;
            }
            if (this.strict || this._uFlag) {
                this.raise("Invalid unicode escape");
            }
            this.rewind(start);
        }
        return false;
    }
    eatIdentityEscape() {
        if (this._uFlag) {
            if (this.eatSyntaxCharacter()) {
                return true;
            }
            if (this.eat(Solidus)) {
                this._lastIntValue = Solidus;
                return true;
            }
            return false;
        }
        if (this.isValidIdentityEscape(this.currentCodePoint)) {
            this._lastIntValue = this.currentCodePoint;
            this.advance();
            return true;
        }
        return false;
    }
    isValidIdentityEscape(cp) {
        if (cp === -1) {
            return false;
        }
        if (this.strict) {
            return !isIdContinue(cp);
        }
        return (cp !== LatinSmallLetterC &&
            (!this._nFlag || cp !== LatinSmallLetterK));
    }
    eatDecimalEscape() {
        this._lastIntValue = 0;
        let cp = this.currentCodePoint;
        if (cp >= DigitOne && cp <= DigitNine) {
            do {
                this._lastIntValue = 10 * this._lastIntValue + (cp - DigitZero);
                this.advance();
            } while ((cp = this.currentCodePoint) >= DigitZero &&
                cp <= DigitNine);
            return true;
        }
        return false;
    }
    eatCharacterClassEscape() {
        const start = this.index;
        if (this.eat(LatinSmallLetterD)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "digit", false);
            return true;
        }
        if (this.eat(LatinCapitalLetterD)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "digit", true);
            return true;
        }
        if (this.eat(LatinSmallLetterS)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "space", false);
            return true;
        }
        if (this.eat(LatinCapitalLetterS)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "space", true);
            return true;
        }
        if (this.eat(LatinSmallLetterW)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "word", false);
            return true;
        }
        if (this.eat(LatinCapitalLetterW)) {
            this._lastIntValue = -1;
            this.onEscapeCharacterSet(start - 1, this.index, "word", true);
            return true;
        }
        let negate = false;
        if (this._uFlag &&
            this.ecmaVersion >= 2018 &&
            (this.eat(LatinSmallLetterP) ||
                (negate = this.eat(LatinCapitalLetterP)))) {
            this._lastIntValue = -1;
            if (this.eat(LeftCurlyBracket) &&
                this.eatUnicodePropertyValueExpression() &&
                this.eat(RightCurlyBracket)) {
                this.onUnicodePropertyCharacterSet(start - 1, this.index, "property", this._lastKeyValue, this._lastValValue || null, negate);
                return true;
            }
            this.raise("Invalid property name");
        }
        return false;
    }
    eatUnicodePropertyValueExpression() {
        const start = this.index;
        if (this.eatUnicodePropertyName() && this.eat(EqualsSign)) {
            this._lastKeyValue = this._lastStrValue;
            if (this.eatUnicodePropertyValue()) {
                this._lastValValue = this._lastStrValue;
                if (isValidUnicodeProperty(this.ecmaVersion, this._lastKeyValue, this._lastValValue)) {
                    return true;
                }
                this.raise("Invalid property name");
            }
        }
        this.rewind(start);
        if (this.eatLoneUnicodePropertyNameOrValue()) {
            const nameOrValue = this._lastStrValue;
            if (isValidUnicodeProperty(this.ecmaVersion, "General_Category", nameOrValue)) {
                this._lastKeyValue = "General_Category";
                this._lastValValue = nameOrValue;
                return true;
            }
            if (isValidLoneUnicodeProperty(this.ecmaVersion, nameOrValue)) {
                this._lastKeyValue = nameOrValue;
                this._lastValValue = "";
                return true;
            }
            this.raise("Invalid property name");
        }
        return false;
    }
    eatUnicodePropertyName() {
        this._lastStrValue = "";
        while (isUnicodePropertyNameCharacter(this.currentCodePoint)) {
            this._lastStrValue += String.fromCodePoint(this.currentCodePoint);
            this.advance();
        }
        return this._lastStrValue !== "";
    }
    eatUnicodePropertyValue() {
        this._lastStrValue = "";
        while (isUnicodePropertyValueCharacter(this.currentCodePoint)) {
            this._lastStrValue += String.fromCodePoint(this.currentCodePoint);
            this.advance();
        }
        return this._lastStrValue !== "";
    }
    eatLoneUnicodePropertyNameOrValue() {
        return this.eatUnicodePropertyValue();
    }
    eatCharacterClass() {
        const start = this.index;
        if (this.eat(LeftSquareBracket)) {
            const negate = this.eat(CircumflexAccent);
            this.onCharacterClassEnter(start, negate);
            this.classRanges();
            if (!this.eat(RightSquareBracket)) {
                this.raise("Unterminated character class");
            }
            this.onCharacterClassLeave(start, this.index, negate);
            return true;
        }
        return false;
    }
    classRanges() {
        let start = this.index;
        while (this.eatClassAtom()) {
            const left = this._lastIntValue;
            const hyphenStart = this.index;
            if (this.eat(HyphenMinus)) {
                this.onCharacter(hyphenStart, this.index, HyphenMinus);
                if (this.eatClassAtom()) {
                    const right = this._lastIntValue;
                    if (left === -1 || right === -1) {
                        if (this.strict) {
                            this.raise("Invalid character class");
                        }
                    }
                    else if (left > right) {
                        this.raise("Range out of order in character class");
                    }
                    else {
                        this.onCharacterClassRange(start, this.index, left, right);
                    }
                }
            }
            start = this.index;
        }
    }
    eatClassAtom() {
        const start = this.index;
        if (this.eat(ReverseSolidus)) {
            if (this.eatClassEscape()) {
                return true;
            }
            if (this._uFlag) {
                this.raise("Invalid escape");
            }
            this.rewind(start);
        }
        const cp = this.currentCodePoint;
        if (cp !== -1 && cp !== RightSquareBracket) {
            this.advance();
            this._lastIntValue = cp;
            this.onCharacter(start, this.index, cp);
            return true;
        }
        return false;
    }
    eatClassEscape() {
        const start = this.index;
        if (this.eat(LatinSmallLetterB)) {
            this._lastIntValue = Backspace;
            this.onCharacter(start - 1, this.index, Backspace);
            return true;
        }
        if (this._uFlag && this.eat(HyphenMinus)) {
            this._lastIntValue = HyphenMinus;
            this.onCharacter(start - 1, this.index, HyphenMinus);
            return true;
        }
        if (!this._uFlag && this.eat(LatinSmallLetterC)) {
            if (this.eatClassControlLetter()) {
                this.onCharacter(start - 1, this.index, this._lastIntValue);
                return true;
            }
            this.rewind(start);
        }
        return this.eatCharacterClassEscape() || this.eatCharacterEscape();
    }
    eatClassControlLetter() {
        const cp = this.currentCodePoint;
        if (isDecimalDigit(cp) || cp === LowLine) {
            this.advance();
            this._lastIntValue = cp % 0x20;
            return true;
        }
        return false;
    }
    eatHexEscapeSequence() {
        const start = this.index;
        if (this.eat(LatinSmallLetterX)) {
            if (this.eatFixedHexDigits(2)) {
                return true;
            }
            if (this._uFlag) {
                this.raise("Invalid escape");
            }
            this.rewind(start);
        }
        return false;
    }
    eatDecimalDigits() {
        const start = this.index;
        this._lastIntValue = 0;
        while (isDecimalDigit(this.currentCodePoint)) {
            this._lastIntValue =
                10 * this._lastIntValue + digitToInt(this.currentCodePoint);
            this.advance();
        }
        return this.index !== start;
    }
    eatHexDigits() {
        const start = this.index;
        this._lastIntValue = 0;
        while (isHexDigit(this.currentCodePoint)) {
            this._lastIntValue =
                16 * this._lastIntValue + digitToInt(this.currentCodePoint);
            this.advance();
        }
        return this.index !== start;
    }
    eatLegacyOctalEscapeSequence() {
        if (this.eatOctalDigit()) {
            const n1 = this._lastIntValue;
            if (this.eatOctalDigit()) {
                const n2 = this._lastIntValue;
                if (n1 <= 3 && this.eatOctalDigit()) {
                    this._lastIntValue = n1 * 64 + n2 * 8 + this._lastIntValue;
                }
                else {
                    this._lastIntValue = n1 * 8 + n2;
                }
            }
            else {
                this._lastIntValue = n1;
            }
            return true;
        }
        return false;
    }
    eatOctalDigit() {
        const cp = this.currentCodePoint;
        if (isOctalDigit(cp)) {
            this.advance();
            this._lastIntValue = cp - DigitZero;
            return true;
        }
        this._lastIntValue = 0;
        return false;
    }
    eatFixedHexDigits(length) {
        const start = this.index;
        this._lastIntValue = 0;
        for (let i = 0; i < length; ++i) {
            const cp = this.currentCodePoint;
            if (!isHexDigit(cp)) {
                this.rewind(start);
                return false;
            }
            this._lastIntValue = 16 * this._lastIntValue + digitToInt(cp);
            this.advance();
        }
        return true;
    }
}

const DummyPattern = {};
const DummyFlags = {};
const DummyCapturingGroup = {};
class RegExpParserState {
    constructor(options) {
        this._node = DummyPattern;
        this._flags = DummyFlags;
        this._backreferences = [];
        this._capturingGroups = [];
        this.source = "";
        this.strict = Boolean(options && options.strict);
        this.ecmaVersion = (options && options.ecmaVersion) || 2020;
    }
    get pattern() {
        if (this._node.type !== "Pattern") {
            throw new Error("UnknownError");
        }
        return this._node;
    }
    get flags() {
        if (this._flags.type !== "Flags") {
            throw new Error("UnknownError");
        }
        return this._flags;
    }
    onFlags(start, end, global, ignoreCase, multiline, unicode, sticky, dotAll) {
        this._flags = {
            type: "Flags",
            parent: null,
            start,
            end,
            raw: this.source.slice(start, end),
            global,
            ignoreCase,
            multiline,
            unicode,
            sticky,
            dotAll,
        };
    }
    onPatternEnter(start) {
        this._node = {
            type: "Pattern",
            parent: null,
            start,
            end: start,
            raw: "",
            alternatives: [],
        };
        this._backreferences.length = 0;
        this._capturingGroups.length = 0;
    }
    onPatternLeave(start, end) {
        this._node.end = end;
        this._node.raw = this.source.slice(start, end);
        for (const reference of this._backreferences) {
            const ref = reference.ref;
            const group = typeof ref === "number"
                ? this._capturingGroups[ref - 1]
                : this._capturingGroups.find(g => g.name === ref);
            reference.resolved = group;
            group.references.push(reference);
        }
    }
    onAlternativeEnter(start) {
        const parent = this._node;
        if (parent.type !== "Assertion" &&
            parent.type !== "CapturingGroup" &&
            parent.type !== "Group" &&
            parent.type !== "Pattern") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "Alternative",
            parent,
            start,
            end: start,
            raw: "",
            elements: [],
        };
        parent.alternatives.push(this._node);
    }
    onAlternativeLeave(start, end) {
        const node = this._node;
        if (node.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onGroupEnter(start) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "Group",
            parent,
            start,
            end: start,
            raw: "",
            alternatives: [],
        };
        parent.elements.push(this._node);
    }
    onGroupLeave(start, end) {
        const node = this._node;
        if (node.type !== "Group" || node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onCapturingGroupEnter(start, name) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "CapturingGroup",
            parent,
            start,
            end: start,
            raw: "",
            name,
            alternatives: [],
            references: [],
        };
        parent.elements.push(this._node);
        this._capturingGroups.push(this._node);
    }
    onCapturingGroupLeave(start, end) {
        const node = this._node;
        if (node.type !== "CapturingGroup" ||
            node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onQuantifier(start, end, min, max, greedy) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        const element = parent.elements.pop();
        if (element == null ||
            element.type === "Quantifier" ||
            (element.type === "Assertion" && element.kind !== "lookahead")) {
            throw new Error("UnknownError");
        }
        const node = {
            type: "Quantifier",
            parent,
            start: element.start,
            end,
            raw: this.source.slice(element.start, end),
            min,
            max,
            greedy,
            element,
        };
        parent.elements.push(node);
        element.parent = node;
    }
    onLookaroundAssertionEnter(start, kind, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        const node = (this._node = {
            type: "Assertion",
            parent,
            start,
            end: start,
            raw: "",
            kind,
            negate,
            alternatives: [],
        });
        parent.elements.push(node);
    }
    onLookaroundAssertionLeave(start, end) {
        const node = this._node;
        if (node.type !== "Assertion" || node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onEdgeAssertion(start, end, kind) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "Assertion",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
        });
    }
    onWordBoundaryAssertion(start, end, kind, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "Assertion",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
            negate,
        });
    }
    onAnyCharacterSet(start, end, kind) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "CharacterSet",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
        });
    }
    onEscapeCharacterSet(start, end, kind, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative" && parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "CharacterSet",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
            negate,
        });
    }
    onUnicodePropertyCharacterSet(start, end, kind, key, value, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative" && parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "CharacterSet",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            kind,
            key,
            value,
            negate,
        });
    }
    onCharacter(start, end, value) {
        const parent = this._node;
        if (parent.type !== "Alternative" && parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        parent.elements.push({
            type: "Character",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            value,
        });
    }
    onBackreference(start, end, ref) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        const node = {
            type: "Backreference",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            ref,
            resolved: DummyCapturingGroup,
        };
        parent.elements.push(node);
        this._backreferences.push(node);
    }
    onCharacterClassEnter(start, negate) {
        const parent = this._node;
        if (parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        this._node = {
            type: "CharacterClass",
            parent,
            start,
            end: start,
            raw: "",
            negate,
            elements: [],
        };
        parent.elements.push(this._node);
    }
    onCharacterClassLeave(start, end) {
        const node = this._node;
        if (node.type !== "CharacterClass" ||
            node.parent.type !== "Alternative") {
            throw new Error("UnknownError");
        }
        node.end = end;
        node.raw = this.source.slice(start, end);
        this._node = node.parent;
    }
    onCharacterClassRange(start, end) {
        const parent = this._node;
        if (parent.type !== "CharacterClass") {
            throw new Error("UnknownError");
        }
        const elements = parent.elements;
        const max = elements.pop();
        const hyphen = elements.pop();
        const min = elements.pop();
        if (!min ||
            !max ||
            !hyphen ||
            min.type !== "Character" ||
            max.type !== "Character" ||
            hyphen.type !== "Character" ||
            hyphen.value !== HyphenMinus) {
            throw new Error("UnknownError");
        }
        const node = {
            type: "CharacterClassRange",
            parent,
            start,
            end,
            raw: this.source.slice(start, end),
            min,
            max,
        };
        min.parent = node;
        max.parent = node;
        elements.push(node);
    }
}
class RegExpParser {
    constructor(options) {
        this._state = new RegExpParserState(options);
        this._validator = new RegExpValidator(this._state);
    }
    parseLiteral(source, start = 0, end = source.length) {
        this._state.source = source;
        this._validator.validateLiteral(source, start, end);
        const pattern = this._state.pattern;
        const flags = this._state.flags;
        const literal = {
            type: "RegExpLiteral",
            parent: null,
            start,
            end,
            raw: source,
            pattern,
            flags,
        };
        pattern.parent = literal;
        flags.parent = literal;
        return literal;
    }
    parseFlags(source, start = 0, end = source.length) {
        this._state.source = source;
        this._validator.validateFlags(source, start, end);
        return this._state.flags;
    }
    parsePattern(source, start = 0, end = source.length, uFlag = false) {
        this._state.source = source;
        this._validator.validatePattern(source, start, end, uFlag);
        return this._state.pattern;
    }
}

class RegExpVisitor {
    constructor(handlers) {
        this._handlers = handlers;
    }
    visit(node) {
        switch (node.type) {
            case "Alternative":
                this.visitAlternative(node);
                break;
            case "Assertion":
                this.visitAssertion(node);
                break;
            case "Backreference":
                this.visitBackreference(node);
                break;
            case "CapturingGroup":
                this.visitCapturingGroup(node);
                break;
            case "Character":
                this.visitCharacter(node);
                break;
            case "CharacterClass":
                this.visitCharacterClass(node);
                break;
            case "CharacterClassRange":
                this.visitCharacterClassRange(node);
                break;
            case "CharacterSet":
                this.visitCharacterSet(node);
                break;
            case "Flags":
                this.visitFlags(node);
                break;
            case "Group":
                this.visitGroup(node);
                break;
            case "Pattern":
                this.visitPattern(node);
                break;
            case "Quantifier":
                this.visitQuantifier(node);
                break;
            case "RegExpLiteral":
                this.visitRegExpLiteral(node);
                break;
            default:
                throw new Error(`Unknown type: ${node.type}`);
        }
    }
    visitAlternative(node) {
        if (this._handlers.onAlternativeEnter) {
            this._handlers.onAlternativeEnter(node);
        }
        node.elements.forEach(this.visit, this);
        if (this._handlers.onAlternativeLeave) {
            this._handlers.onAlternativeLeave(node);
        }
    }
    visitAssertion(node) {
        if (this._handlers.onAssertionEnter) {
            this._handlers.onAssertionEnter(node);
        }
        if (node.kind === "lookahead" || node.kind === "lookbehind") {
            node.alternatives.forEach(this.visit, this);
        }
        if (this._handlers.onAssertionLeave) {
            this._handlers.onAssertionLeave(node);
        }
    }
    visitBackreference(node) {
        if (this._handlers.onBackreferenceEnter) {
            this._handlers.onBackreferenceEnter(node);
        }
        if (this._handlers.onBackreferenceLeave) {
            this._handlers.onBackreferenceLeave(node);
        }
    }
    visitCapturingGroup(node) {
        if (this._handlers.onCapturingGroupEnter) {
            this._handlers.onCapturingGroupEnter(node);
        }
        node.alternatives.forEach(this.visit, this);
        if (this._handlers.onCapturingGroupLeave) {
            this._handlers.onCapturingGroupLeave(node);
        }
    }
    visitCharacter(node) {
        if (this._handlers.onCharacterEnter) {
            this._handlers.onCharacterEnter(node);
        }
        if (this._handlers.onCharacterLeave) {
            this._handlers.onCharacterLeave(node);
        }
    }
    visitCharacterClass(node) {
        if (this._handlers.onCharacterClassEnter) {
            this._handlers.onCharacterClassEnter(node);
        }
        node.elements.forEach(this.visit, this);
        if (this._handlers.onCharacterClassLeave) {
            this._handlers.onCharacterClassLeave(node);
        }
    }
    visitCharacterClassRange(node) {
        if (this._handlers.onCharacterClassRangeEnter) {
            this._handlers.onCharacterClassRangeEnter(node);
        }
        this.visitCharacter(node.min);
        this.visitCharacter(node.max);
        if (this._handlers.onCharacterClassRangeLeave) {
            this._handlers.onCharacterClassRangeLeave(node);
        }
    }
    visitCharacterSet(node) {
        if (this._handlers.onCharacterSetEnter) {
            this._handlers.onCharacterSetEnter(node);
        }
        if (this._handlers.onCharacterSetLeave) {
            this._handlers.onCharacterSetLeave(node);
        }
    }
    visitFlags(node) {
        if (this._handlers.onFlagsEnter) {
            this._handlers.onFlagsEnter(node);
        }
        if (this._handlers.onFlagsLeave) {
            this._handlers.onFlagsLeave(node);
        }
    }
    visitGroup(node) {
        if (this._handlers.onGroupEnter) {
            this._handlers.onGroupEnter(node);
        }
        node.alternatives.forEach(this.visit, this);
        if (this._handlers.onGroupLeave) {
            this._handlers.onGroupLeave(node);
        }
    }
    visitPattern(node) {
        if (this._handlers.onPatternEnter) {
            this._handlers.onPatternEnter(node);
        }
        node.alternatives.forEach(this.visit, this);
        if (this._handlers.onPatternLeave) {
            this._handlers.onPatternLeave(node);
        }
    }
    visitQuantifier(node) {
        if (this._handlers.onQuantifierEnter) {
            this._handlers.onQuantifierEnter(node);
        }
        this.visit(node.element);
        if (this._handlers.onQuantifierLeave) {
            this._handlers.onQuantifierLeave(node);
        }
    }
    visitRegExpLiteral(node) {
        if (this._handlers.onRegExpLiteralEnter) {
            this._handlers.onRegExpLiteralEnter(node);
        }
        this.visitPattern(node.pattern);
        this.visitFlags(node.flags);
        if (this._handlers.onRegExpLiteralLeave) {
            this._handlers.onRegExpLiteralLeave(node);
        }
    }
}

function parseRegExpLiteral(source, options) {
    return new RegExpParser(options).parseLiteral(String(source));
}
function validateRegExpLiteral(source, options) {
    return new RegExpValidator(options).validateLiteral(source);
}
function visitRegExpAST(node, handlers) {
    new RegExpVisitor(handlers).visit(node);
}

exports.AST = ast;
exports.RegExpParser = RegExpParser;
exports.RegExpValidator = RegExpValidator;
exports.parseRegExpLiteral = parseRegExpLiteral;
exports.validateRegExpLiteral = validateRegExpLiteral;
exports.visitRegExpAST = visitRegExpAST;
//# sourceMappingURL=index.js.map
