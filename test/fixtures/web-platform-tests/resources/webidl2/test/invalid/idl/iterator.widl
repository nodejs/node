interface SessionManager {
  Session getSessionForUser(DOMString username);
  readonly attribute unsigned long sessionCount;

  Session iterator;
};

interface Session {
  readonly attribute DOMString username;
  // ...
};

interface SessionManager2 {
  Session2 getSessionForUser(DOMString username);
  readonly attribute unsigned long sessionCount;

  Session2 iterator = SessionIterator;
};

interface Session2 {
  readonly attribute DOMString username;
  // ...
};

interface SessionIterator {
  readonly attribute unsigned long remainingSessions;
};

     interface NodeList {
       Node iterator = NodeIterator;
     };

     interface NodeIterator {
       Node iterator object;
     };