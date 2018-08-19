
      interface Point {
        attribute float x;
        attribute float y;
      };


      interface Rect {
        attribute Point topleft;
        attribute Point bottomright;
      };

  interface Widget {
    typedef sequence<Point> PointSequence;

    readonly attribute Rect bounds;

    boolean pointWithinBounds(Point p);
    boolean allPointsWithinBounds(PointSequence ps);
  };

  typedef [Clamp] octet value;