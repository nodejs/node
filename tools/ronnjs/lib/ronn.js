/* ronn.js version 0.1
 * Copyright : 2010 Jérémy Lal <kapouer@melix.org>
 * License : MIT
 */

var md = require(__dirname + '/ext/markdown');
var sys = require('sys');

/* exports Ronn class
 * usage :
 * var ronn = new Ronn(rofftext, "1.0", "my manual name", "2010-12-25");
 * ronn.roff();
 * ronn.html();
 * ronn.fragment();
 */

exports.Ronn = function(text, version, manual, date) {
	if (!manual) manual = "";
	if (!version) version = "";
	if (!date) date = new Date();
	else date = new Date(date + " GMT");

	var gHtml = md.toHTMLTree(text);

	this.roff = function() {
		return blockFilter("", gHtml, {parent:null, previous:null, position:null});
	};

	this.html = function() {
		return toHTML(gHtml);
	};

	this.fragment = function() {
		return toHTMLfragment(gHtml);
	};

	function blockFilter(out, node, context) {
		if (typeof node == "string") {
			if (!node.match(/^\s*$/m)) sys.debug("unexpected text: " + node);
			return out;
		}
		var tag = node.shift();	
		var attributes = null;
		if (node.length && typeof node[0] === "object" && !(node[0] instanceof Array)) {
			attributes = node.shift();
		}
		var fParent = context.parent;
		var fPrevious = context.previous;
		context.previous = null;
		context.parent = tag;
		switch (tag) {
			case "html":
				out = comment(out, "Generated with Ronnjs/v0.1");
				out = comment(out, "http://github.com/kapouer/ronnjs/");
				while (node.length) out = blockFilter(out, node.shift(), context);
			break;
			case "h1":
				var fTagline = node.shift();
				var fMatch = /([\w_.\[\]~+=@:-]+)\s*\((\d\w*)\)\s*-+\s*(.*)/.exec(fTagline);
				var fName, fSection;
				if (fMatch != null) {
					fName = fMatch[1];
					fSection = fMatch[2];
					fTagline = fMatch[3];
				} else {
					fMatch = /([\w_.\[\]~+=@:-]+)\s+-+\s+(.*)/.exec(fTagline);
					if (fMatch != null) {
						fName = fMatch[1];
						fTagline = fMatch[2];
					}
				}
				if (fMatch == null) {
					fName = "";
					fSection = "";
					fName = "";
				}
				out = macro(out, "TH", [
					quote(esc(fName.toUpperCase()))
					, quote(fSection)
					, quote(manDate(date))
					, quote(version)
					, quote(manual)
				]);
				out = macro(out, "SH", quote("NAME"));
				out += "\\fB" + fName + "\\fR";
				if (fTagline.length > 0) out += " \\-\\- " + esc(fTagline);
			break;
			case "h2":
				out = macro(out, "SH", quote(esc(toHTML(node.shift()))));
			break;
			case "h3":
				out = macro(out, "SS", quote(esc(toHTML(node.shift()))));
			break;
			case "hr":
				out = macro(out, "HR");
			break;
			case "p":
				if (fPrevious && fParent && (fParent == "dd" || fParent == "li"))
					out = macro(out, "IP");
				else if (fPrevious && !(fPrevious == "h1" || fPrevious == "h2" || fPrevious == "h3"))
					out = macro(out, "P");
				out = callInlineChildren(out, node, context);
			break;
			case "pre":
				var indent = (fPrevious == null || !(fPrevious == "h1" || fPrevious == "h2" || fPrevious == "h3"));
				if (indent) out = macro(out, "IP", [quote(""), 4]);
				out = macro(out, "nf");
				out = callInlineChildren(out, node, context);
				out = macro(out, "fi");
				if (indent) out = macro(out, "IP", [quote(""), 0]);
			break;
			case "dl":
				out = macro(out, "TP");
				while (node.length) out = blockFilter(out, node.shift(), context);
			break;
			case "dt":
				if (fPrevious != null) out = macro(out, "TP");
				out = callInlineChildren(out, node, context);
				out += "\n";
			break;
			case "dd":
				if (containsTag(node, {'p':true})) {
					while (node.length) out = blockFilter(out, node.shift(), context);
				} else {
					out = callInlineChildren(out, node, context);
				}
				out += "\n";
			break;
			case "ol":
			case "ul":
				context.position = 0;
				while (node.length) {
					out = blockFilter(out, node.shift(), context);
				}
				context.position = null;
				out = macro(out, "IP", [quote(""), 0]);
			break;
			case "li":
				if (fParent == "ol") {
					context.position += 1;
					out = macro(out, "IP", [quote(context.position), 4]);
				} else if (fParent == "ul") {
					out = macro(out, "IP", [quote("\\(bu"), 4]);
				}
				if (containsTag(node, {"p":true, "ol":true, "ul":true, "dl":true, "div":true})) {
					while (node.length) out = blockFilter(out, node.shift(), context);
				} else {
					out = callInlineChildren(out, node, context);
				}
				out += "\n";
			break;
			default:
				sys.debug("unrecognized block tag: " + tag);
			break;
		}
		context.parent = fParent;
		context.previous = tag;
		return out;
	}

	function callInlineChildren(out, node, context) {
		while (node.length) {
			var lChild = node.shift();
			if (node.length > 0) context.hasNext = true;
			else context.hasNext = false;
			out = inlineFilter(out, lChild, context);
		}
		return out;
	}

	function inlineFilter(out, node, context) {
		if (typeof node == "string") {
			if (context.previous && context.previous == "br") node = node.replace(/^\n+/gm, '');
			if (context.parent == "pre") {
				// do nothing
			} else if (context.previous == null && !context.hasNext) {
				node = node.replace(/\n+$/gm, '');
			} else {
				node = node.replace(/\n+$/gm, ' ');
			}
			out += esc(node);
			return out;
		}
		var tag = node.shift();	
		var attributes = null;
		if (node.length && typeof node[0] === "object" && !(node[0] instanceof Array)) {
			attributes = node.shift();
		}
		var fParent = context.parent;
		var fPrevious = context.previous;
		context.parent = tag;
		context.previous = null;
		switch(tag) {
			case "code":
				if (fParent == "pre") {
					out = callInlineChildren(out, node, context);
				} else {
					out += '\\fB';
					out = callInlineChildren(out, node, context);
					out += '\\fR';
				}
			break;
			case "b":
			case "strong":
			case "kbd":
			case "samp":
				out += '\\fB';
				out = callInlineChildren(out, node, context);
				out += '\\fR';
			break;
			case "var":
			case "em":
			case "i":
			case "u":
				out += '\\fI';
				out = callInlineChildren(out, node, context);
				out += '\\fR';
			break;
			case "br":
				out = macro(out, "br");
			break;
			case "a":
				var fStr = node[0];
				var fHref = attributes['href'];
				if (fHref == fStr || decodeURI(fHref) == "mailto:" + decodeURI(fStr)) {
					out += '\\fI';
					out = callInlineChildren(out, node, context);
					out += '\\fR';
				} else {
					out = callInlineChildren(out, node, context);
					out += " ";
					out += '\\fI';
					out += esc(fHref);
					out += '\\fR';
				}
			break;
			default:
				sys.debug("unrecognized inline tag: " + tag);
			break;
		}
		context.parent = fParent;
		context.previous = tag;
		return out;
	}

	function containsTag(node, tags) {
		// browse ml tree searching for tags (hash {tag : true, ...})
		if (typeof node == "string") return false;
		var jml = node.slice(0);
		if (jml.length == 0) return false;
		else while (jml.length && jml[0] instanceof Array) {
			if (containsTag(jml.shift(), tags)) return true;
		}
		var tag = jml.shift();
		if (tags[tag] === true) return true;
		if (jml.length && typeof jml[0] === "object" && !(jml[0] instanceof Array)) {
			// skip attributes
			jml.shift();
		}
		// children
		if (jml.length) {
			if (containsTag(jml.shift(), tags)) return true;
		}
		// siblings
		if (jml.length) return containsTag(jml, tags);
	}

	function toHTML(node) {
		// problème ici : les & sont remplacés par des &amp;
		return md.renderJsonML(node, {root:true});
	}

	function toHTMLfragment(node) {
		return md.renderJsonML(node);
	}

	function comment(out, str) {
		return writeln(out, '.\\" ' + str);
	}

	function quote(str) {
		return '"' + str + '"';
	}

	function esc(str) {
		return str
			.replace(/\\/gm, "\\\\")
			.replace(/-/gm, "\\-")
			.replace(/^\./gm, "\\|.")
			.replace(/\./gm, "\\.")
			.replace(/'/gm, "\\'")
			;
	}

	function writeln(out, str) {
		if (out.length && out[out.length - 1] != "\n") out += "\n";
		out += str + "\n";
		return out;
	}

	function macro(out, name, list) {
		var fText = ".\n." + name;
		if (list != null) {
			if (typeof list == "string") {
				fText += ' ' + list;
			} else {
				for (var i=0, len=list.length; i < len; i++) {
					var item = list[i];
					if (item == null) continue;
					fText += ' ' + item;
				}
			}
		}
		return writeln(out, fText);
	}

	function manDate(pDate) {
		var fMonth = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"][pDate.getMonth()];
		return fMonth + " " + pDate.getFullYear();
	}
};
