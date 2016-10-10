/** 
 * @license Highcharts JS v2.1.2 (2011-01-12)
 * Prototype adapter
 * 
 * @author Michael Nelson, Torstein Hønsi.
 * 
 * Feel free to use and modify this script.
 * Highcharts license: www.highcharts.com/license.
 */

/* 
 * Known issues: 
 *    - Some grid lines land in wrong position - http://jsfiddle.net/highcharts/jaRhY/28
 */

// JSLint options:
/*jslint forin: true */
/*global Effect, Class, Highcharts, Event, $, $A */

// Adapter interface between prototype and the Highcarts charting library
var HighchartsAdapter = (function() {

var hasEffect = typeof Effect != 'undefined';

return { 
	
	init: function() {
		
		if (hasEffect) {
			/**
			 * Animation for Highcharts SVG element wrappers only
			 * @param {Object} element
			 * @param {Object} attribute
			 * @param {Object} to
			 * @param {Object} options
			 */
			Effect.HighchartsTransition = Class.create(Effect.Base, {
				initialize: function(element, attr, to, options){
					var from,
						opts;
					
					this.element = element;
					
					from = element.attr(attr);
					
					// special treatment for paths
					if (attr == 'd') {
						this.paths = Highcharts.pathAnim.init(
							element, 
							element.d,
							to
						);
						this.toD = to;
						
						
						// fake values in order to read relative position as a float in update
						from = 0; 
						to = 1;				
					}
					
					opts = Object.extend((options || {}), {
						from: from,
						to: to,
						attribute: attr
					});
					this.start(opts);
				},
				setup: function(){
					HighchartsAdapter._extend(this.element);
					this.element._highchart_animation = this;
				},
				update: function(position) {
					var paths = this.paths;
					
					if (paths) {
						position = Highcharts.pathAnim.step(paths[0], paths[1], position, this.toD);
					}
					
					this.element.attr(this.options.attribute, position);
				},
				finish: function(){
					this.element._highchart_animation = null;
				}
			});
		}
	},
	
	// el needs an event to be attached. el is not necessarily a dom element
	addEvent: function(el, event, fn) {
		if (el.addEventListener || el.attachEvent) {
			Event.observe($(el), event, fn);
		
		} else {
			HighchartsAdapter._extend(el);
			el._highcharts_observe(event, fn);
		}
	},
	
	// motion makes things pretty. use it if effects is loaded, if not... still get to the end result.
	animate: function(el, params, options) { 
		var key,
			fx;
		
		// default options
		options = options || {};
		options.delay = 0;
		options.duration = (options.duration || 500) / 1000;
		
		// animate wrappers and DOM elements
		if (hasEffect) { 
			for (key in params) {
				fx = new Effect.HighchartsTransition($(el), key, params[key], options);
			}
		} else { 
			for (key in params) {
				el.attr(key, params[key]);
			}
		}
		
		if (!el.attr) {
			throw 'Todo: implement animate DOM objects';
		}
	},
	
	// this only occurs in higcharts 2.0+
	stop: function(el){
		if (el._highcharts_extended && el._highchart_animation) {
			el._highchart_animation.cancel();
		}
	},
	
	// um.. each
	each: function(arr, fn){
		$A(arr).each(fn);
	},
	
	// fire an event based on an event name (event) and an object (el).
	// again, el may not be a dom element
	fireEvent: function(el, event, eventArguments, defaultFunction){
		if (event.preventDefault) {
			defaultFunction = null;
		}
		
		if (el.fire) {
			el.fire(event, eventArguments);
		} else if (el._highcharts_extended) {
			el._highcharts_fire(event, eventArguments);
		}
		
		if (defaultFunction) { 
			defaultFunction(eventArguments);
		}
	},
	
	removeEvent: function(el, event, handler){
		if ($(el).stopObserving) {
			el.stopObserving(el, event, handler);
			
		} else {
			HighchartsAdapter._extend(el);
			el._highcharts_stop_observing(event, handler);
		}
	},
	
	// um, grep
	grep: function(arr, fn){
		return arr.findAll(fn);
	},
	
	// change leftPadding to left-padding
	hyphenate: function(str){
		return str.replace(/([A-Z])/g, function(a, b){
			return '-' + b.toLowerCase();
		});
	},
	
	// um, map
	map: function(arr, fn){
		return arr.map(fn);
	},
	
	// deep merge. merge({a : 'a', b : {b1 : 'b1', b2 : 'b2'}}, {b : {b2 : 'b2_prime'}, c : 'c'}) => {a : 'a', b : {b1 : 'b1', b2 : 'b2_prime'}, c : 'c'}
	merge: function(){
		function doCopy(copy, original) {
			var value,
				key,
				undef,
				nil,
				same,
				obj,
				arr,
				node;
				
			for (key in original) {
				value = original[key];
				undef = typeof(value) === 'undefined';
				nil = value === null;
				same = original === copy[key];
				
				if (undef || nil || same) { 
					continue;
				}
				
				obj = typeof(value) === 'object';
				arr = value && obj && value.constructor == Array;
				node = !!value.nodeType;
				
				if (obj && !arr && !node) {
					copy[key] = doCopy(typeof copy[key] == 'object' ? copy[key] : {}, value);
				}
				else {
					copy[key] = original[key];
				}
			}
			return copy;
		}
		
		var args = arguments, retVal = {};
		
		for (var i = 0; i < args.length; i++) {
			retVal = doCopy(retVal, args[i]);			
		}
		
		return retVal;
	},
	
	// extend an object to handle highchart events (highchart objects, not svg elements). 
	// this is a very simple way of handling events but whatever, it works (i think)
	_extend: function(object){
		if (!object._highcharts_extended) {
			Object.extend(object, {
				_highchart_events: {},
				_highchart_animation: null,
				_highcharts_extended: true,
				_highcharts_observe: function(name, fn){
					this._highchart_events[name] = [this._highchart_events[name], fn].compact().flatten();
				},
				_highcharts_stop_observing: function(name, fn){
					this._highchart_events[name] = [this._highchart_events[name]].compact().flatten().without(fn);
				},
				_highcharts_fire: function(name, args){
					(this._highchart_events[name] || []).each(function(fn){
						if (args && args.stopped) {
							return; // "throw $break" wasn't working. i think because of the scope of 'this'.
						}
						fn.bind(this)(args);
					}
.bind(this));
				}
			});
		}
	}
};
})();
