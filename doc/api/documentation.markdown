# About this Documentation

<!-- type=misc -->

The goal of this documentation is to comprehensively explain the Node.js
API, both from a reference as well as a conceptual point of view.  Each
section describes a built-in module or high-level concept.

Where appropriate, property types, method arguments, and the arguments
provided to event handlers are detailed in a list underneath the topic
heading.

Every `.html` document has a corresponding `.json` document presenting
the same information in a structured manner.  This feature is
experimental, and added for the benefit of IDEs and other utilities that
wish to do programmatic things with the documentation.

Every `.html` and `.json` file is generated based on the corresponding
`.markdown` file in the `doc/api/` folder in node's source tree.  The
documentation is generated using the `tools/doc/generate.js` program.
The HTML template is located at `doc/template.html`.

## Stability Index

<!--type=misc-->

Throughout the documentation, you will see indications of a section's
stability.  The Node.js API is still somewhat changing, and as it
matures, certain parts are more reliable than others.  Some are so
proven, and so relied upon, that they are unlikely to ever change at
all.  Others are brand new and experimental, or known to be hazardous
and in the process of being redesigned.

The notices look like this:

    Stability: 1 Experimental

The stability indices are as follows:

* **0 - Deprecated**  This feature is known to be problematic, and changes are
planned.  Do not rely on it.  Use of the feature may cause warnings.  Backwards
compatibility should not be expected.

* **1 - Experimental**  This feature was introduced recently, and may change
or be removed in future versions.  Please try it out and provide feedback.
If it addresses a use-case that is important to you, tell the node core team.

* **2 - Unstable**  The API is in the process of settling, but has not yet had
sufficient real-world testing to be considered stable. Backwards-compatibility
will be maintained if reasonable.

* **3 - Stable**  The API has proven satisfactory, but cleanup in the underlying
code may cause minor changes.  Backwards-compatibility is guaranteed.

* **4 - API Frozen**  This API has been tested extensively in production and is
unlikely to ever have to change.

* **5 - Locked**  Unless serious bugs are found, this code will not ever
change.  Please do not suggest changes in this area; they will be refused.

## JSON Output

    Stability: 1 - Experimental

Every HTML file in the markdown has a corresponding JSON file with the
same data.

This feature is new as of node v0.6.12.  It is experimental.
