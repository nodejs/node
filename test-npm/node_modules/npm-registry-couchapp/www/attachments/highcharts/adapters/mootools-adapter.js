/*
 Highcharts JS v2.1.2 (2011-01-12)
 MooTools adapter

 (c) 2010 Torstein H?nsi

 License: www.highcharts.com/license
*/
var HighchartsAdapter={init:function(){var a=Fx.prototype,b=a.start,c=Fx.Morph.prototype,d=c.compute;a.start=function(f){var e=this.element;if(f.d)this.paths=Highcharts.pathAnim.init(e,e.d,this.toD);b.apply(this,arguments)};c.compute=function(f,e,h){var g=this.paths;if(g)this.element.attr("d",Highcharts.pathAnim.step(g[0],g[1],h,this.toD));else return d.apply(this,arguments)}},animate:function(a,b,c){var d=a.attr,f=c&&c.complete;if(d&&!a.setStyle){a.getStyle=a.attr;a.setStyle=function(){var e=arguments;
a.attr.call(a,e[0],e[1][0])};a.$family=a.uid=true}HighchartsAdapter.stop(a);c=new Fx.Morph(d?a:$(a),$extend({transition:Fx.Transitions.Quad.easeInOut},c));if(b.d)c.toD=b.d;f&&c.addEvent("complete",f);c.start(b);a.fx=c},each:$each,map:function(a,b){return a.map(b)},grep:function(a,b){return a.filter(b)},merge:$merge,hyphenate:function(a){return a.hyphenate()},addEvent:function(a,b,c){if(typeof b=="string"){if(b=="unload")b="beforeunload";if(!a.addEvent)if(a.nodeName)a=$(a);else $extend(a,new Events);
a.addEvent(b,c)}},removeEvent:function(a,b,c){if(b){if(b=="unload")b="beforeunload";a.removeEvent(b,c)}},fireEvent:function(a,b,c,d){b=new Event({type:b,target:a});b=$extend(b,c);b.preventDefault=function(){d=null};a.fireEvent&&a.fireEvent(b.type,b);d&&d(b)},stop:function(a){a.fx&&a.fx.cancel()}};
