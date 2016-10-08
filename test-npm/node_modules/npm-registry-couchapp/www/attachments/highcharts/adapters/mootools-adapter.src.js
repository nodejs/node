/** 
 * @license Highcharts JS v2.1.2 (2011-01-12)
 * MooTools adapter
 * 
 * (c) 2010 Torstein Hønsi
 * 
 * License: www.highcharts.com/license
 */

// JSLint options:
/*global Highcharts, Fx, $, $extend, $each, $merge, Events, Event */

var HighchartsAdapter = {
	/**
	 * Initialize the adapter. This is run once as Highcharts is first run.
	 */
	init: function() {
		var fxProto = Fx.prototype,
			fxStart = fxProto.start,
			morphProto = Fx.Morph.prototype,
			morphCompute = morphProto.compute;
			
		// override Fx.start to allow animation of SVG element wrappers
		fxProto.start = function(from, to) {
			var fx = this,
				elem = fx.element;
			
			// special for animating paths
			if (from.d) {
				//this.fromD = this.element.d.split(' ');
				fx.paths = Highcharts.pathAnim.init(
					elem, 
					elem.d, 
					fx.toD
				);
			}
			fxStart.apply(fx, arguments);
		};
		
		// override Fx.step to allow animation of SVG element wrappers
		morphProto.compute = function(from, to, delta) {
			var fx = this,
				paths = fx.paths;
			
			if (paths) {
				fx.element.attr(
					'd', 
					Highcharts.pathAnim.step(paths[0], paths[1], delta, fx.toD)
				);
			} else {
				return morphCompute.apply(fx, arguments);
			}
		};
		
	},	
	
	/**
	 * Animate a HTML element or SVG element wrapper
	 * @param {Object} el
	 * @param {Object} params
	 * @param {Object} options jQuery-like animation options: duration, easing, callback
	 */
	animate: function (el, params, options) {
		var isSVGElement = el.attr,
			effect,
			complete = options && options.complete;
		
		if (isSVGElement && !el.setStyle) {
			// add setStyle and getStyle methods for internal use in Moo
			el.getStyle = el.attr;
			el.setStyle = function() { // property value is given as array in Moo - break it down
				var args = arguments;
				el.attr.call(el, args[0], args[1][0]);
			}
			// dirty hack to trick Moo into handling el as an element wrapper
			el.$family = el.uid = true;
		}
		
		// stop running animations
		HighchartsAdapter.stop(el);
		
		// define and run the effect
		effect = new Fx.Morph(
			isSVGElement ? el : $(el), 
			$extend({
				transition: Fx.Transitions.Quad.easeInOut
			}, options)
		);
		
		// special treatment for paths
		if (params.d) {
			effect.toD = params.d;
		}
		
		// jQuery-like events
		if (complete) {
			effect.addEvent('complete', complete);
		}
		
		// run
		effect.start(params);
		
		// record for use in stop method
		el.fx = effect;
	},
	
	/**
	 * MooTool's each function
	 * 
	 */
	each: $each,
	
	/**
	 * Map an array
	 * @param {Array} arr
	 * @param {Function} fn
	 */
	map: function (arr, fn){
		return arr.map(fn);
	},
	
	/**
	 * Grep or filter an array
	 * @param {Array} arr
	 * @param {Function} fn
	 */
	grep: function(arr, fn) {
		return arr.filter(fn);
	},
	
	/**
	 * Deep merge two objects and return a third
	 */
	merge: $merge,
	
	/**
	 * Hyphenate a string, like minWidth becomes min-width
	 * @param {Object} str
	 */
	hyphenate: function (str){
		return str.hyphenate();
	},
	
	/**
	 * Add an event listener
	 * @param {Object} el HTML element or custom object
	 * @param {String} type Event type
	 * @param {Function} fn Event handler
	 */
	addEvent: function (el, type, fn) {
		if (typeof type == 'string') { // chart broke due to el being string, type function
		
			if (type == 'unload') { // Moo self destructs before custom unload events
				type = 'beforeunload';
			}

			// if the addEvent method is not defined, el is a custom Highcharts object
			// like series or point
			if (!el.addEvent) {
				if (el.nodeName) {
					el = $(el); // a dynamically generated node
				} else {
					$extend(el, new Events()); // a custom object
				}
			}
			
			el.addEvent(type, fn);
		}
	},
	
	removeEvent: function(el, type, fn) {
		if (type) {
			if (type == 'unload') { // Moo self destructs before custom unload events
				type = 'beforeunload';
			}


			el.removeEvent(type, fn);
		}
	},
	
	fireEvent: function(el, event, eventArguments, defaultFunction) {
		// create an event object that keeps all functions		
		event = new Event({ 
			type: event,
			target: el
		});
		event = $extend(event, eventArguments);
		// override the preventDefault function to be able to use
		// this for custom events
		event.preventDefault = function() {
			defaultFunction = null;
		};
		// if fireEvent is not available on the object, there hasn't been added
		// any events to it above
		if (el.fireEvent) {
			el.fireEvent(event.type, event);
		}
		
		// fire the default if it is passed and it is not prevented above
		if (defaultFunction) {
			defaultFunction(event);
		}		
	},
	
	/**
	 * Stop running animations on the object
	 */
	stop: function (el) {
		if (el.fx) {
			el.fx.cancel();
		}
	}
};