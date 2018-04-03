/**
 * @fileoverview Defines environment settings and globals.
 * @author Elan Shanker
 */
"use strict";

//------------------------------------------------------------------------------
// Requirements
//------------------------------------------------------------------------------

const globals = require("globals");

//------------------------------------------------------------------------------
// Public Interface
//------------------------------------------------------------------------------

module.exports = {
    builtin: {
        globals: globals.es5
    },
    browser: {

        /*
         * For backward compatibility.
         * Remove those on the next major release.
         */
        globals: Object.assign(
            {
                AutocompleteErrorEvent: false,
                CDATASection: false,
                ClientRect: false,
                ClientRectList: false,
                CSSAnimation: false,
                CSSTransition: false,
                CSSUnknownRule: false,
                CSSViewportRule: false,
                Debug: false,
                DocumentTimeline: false,
                DOMSettableTokenList: false,
                ElementTimeControl: false,
                FederatedCredential: false,
                FileError: false,
                HTMLAppletElement: false,
                HTMLBlockquoteElement: false,
                HTMLIsIndexElement: false,
                HTMLKeygenElement: false,
                HTMLLayerElement: false,
                IDBEnvironment: false,
                InputMethodContext: false,
                MediaKeyError: false,
                MediaKeyEvent: false,
                MediaKeys: false,
                opera: false,
                PasswordCredential: false,
                ReadableByteStream: false,
                SharedKeyframeList: false,
                showModalDialog: false,
                SiteBoundCredential: false,
                SVGAltGlyphDefElement: false,
                SVGAltGlyphElement: false,
                SVGAltGlyphItemElement: false,
                SVGAnimateColorElement: false,
                SVGAnimatedPathData: false,
                SVGAnimatedPoints: false,
                SVGColor: false,
                SVGColorProfileElement: false,
                SVGColorProfileRule: false,
                SVGCSSRule: false,
                SVGCursorElement: false,
                SVGDocument: false,
                SVGElementInstance: false,
                SVGElementInstanceList: false,
                SVGEvent: false,
                SVGExternalResourcesRequired: false,
                SVGFilterPrimitiveStandardAttributes: false,
                SVGFitToViewBox: false,
                SVGFontElement: false,
                SVGFontFaceElement: false,
                SVGFontFaceFormatElement: false,
                SVGFontFaceNameElement: false,
                SVGFontFaceSrcElement: false,
                SVGFontFaceUriElement: false,
                SVGGlyphElement: false,
                SVGGlyphRefElement: false,
                SVGHKernElement: false,
                SVGICCColor: false,
                SVGLangSpace: false,
                SVGLocatable: false,
                SVGMissingGlyphElement: false,
                SVGPaint: false,
                SVGPathSeg: false,
                SVGPathSegArcAbs: false,
                SVGPathSegArcRel: false,
                SVGPathSegClosePath: false,
                SVGPathSegCurvetoCubicAbs: false,
                SVGPathSegCurvetoCubicRel: false,
                SVGPathSegCurvetoCubicSmoothAbs: false,
                SVGPathSegCurvetoCubicSmoothRel: false,
                SVGPathSegCurvetoQuadraticAbs: false,
                SVGPathSegCurvetoQuadraticRel: false,
                SVGPathSegCurvetoQuadraticSmoothAbs: false,
                SVGPathSegCurvetoQuadraticSmoothRel: false,
                SVGPathSegLinetoAbs: false,
                SVGPathSegLinetoHorizontalAbs: false,
                SVGPathSegLinetoHorizontalRel: false,
                SVGPathSegLinetoRel: false,
                SVGPathSegLinetoVerticalAbs: false,
                SVGPathSegLinetoVerticalRel: false,
                SVGPathSegList: false,
                SVGPathSegMovetoAbs: false,
                SVGPathSegMovetoRel: false,
                SVGRenderingIntent: false,
                SVGStylable: false,
                SVGTests: false,
                SVGTransformable: false,
                SVGTRefElement: false,
                SVGURIReference: false,
                SVGViewSpec: false,
                SVGVKernElement: false,
                SVGZoomAndPan: false,
                SVGZoomEvent: false,
                TimeEvent: false,
                XDomainRequest: false,
                XMLHttpRequestProgressEvent: false,
                XPathException: false,
                XPathNamespace: false,
                XPathNSResolver: false
            },
            globals.browser
        )
    },
    node: {

        /*
         * For backward compatibility.
         * Remove those on the next major release.
         */
        globals: Object.assign(
            { arguments: false, GLOBAL: false, root: false },
            globals.node
        ),
        parserOptions: {
            ecmaFeatures: {
                globalReturn: true
            }
        }
    },
    commonjs: {
        globals: globals.commonjs,
        parserOptions: {
            ecmaFeatures: {
                globalReturn: true
            }
        }
    },
    "shared-node-browser": {
        globals: globals["shared-node-browser"]
    },
    worker: {
        globals: globals.worker
    },
    amd: {
        globals: globals.amd
    },
    mocha: {
        globals: globals.mocha
    },
    jasmine: {
        globals: globals.jasmine
    },
    jest: {

        /*
         * For backward compatibility.
         * Remove those on the next major release.
         */
        globals: Object.assign(
            { check: false, gen: false },
            globals.jest
        )
    },
    phantomjs: {
        globals: globals.phantomjs
    },
    jquery: {
        globals: globals.jquery
    },
    qunit: {
        globals: globals.qunit
    },
    prototypejs: {
        globals: globals.prototypejs
    },
    shelljs: {
        globals: globals.shelljs
    },
    meteor: {
        globals: globals.meteor
    },
    mongo: {
        globals: globals.mongo
    },
    protractor: {
        globals: globals.protractor
    },
    applescript: {
        globals: globals.applescript
    },
    nashorn: {
        globals: globals.nashorn
    },
    serviceworker: {
        globals: globals.serviceworker
    },
    atomtest: {
        globals: globals.atomtest
    },
    embertest: {
        globals: globals.embertest
    },
    webextensions: {
        globals: globals.webextensions
    },
    es6: {
        globals: globals.es2015,
        parserOptions: {
            ecmaVersion: 6
        }
    },
    greasemonkey: {
        globals: globals.greasemonkey
    }
};
