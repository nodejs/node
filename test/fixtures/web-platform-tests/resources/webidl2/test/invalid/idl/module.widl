// Extracted from http://dev.w3.org/2006/webapi/WebIDL/ on 2011-05-06
module gfx {

  module geom {
    interface Shape { /* ... */ };
    interface Rectangle : Shape { /* ... */ };
    interface Path : Shape { /* ... */ };
  };

  interface GraphicsContext {
    void fillShape(geom::Shape s);
    void strokeShape(geom::Shape s);
  };
};

module gui {

  interface Widget { /* ... */  };

  interface Window : Widget {
    gfx::GraphicsContext getGraphicsContext();
  };

  interface Button : Widget { /* ... */  };
};