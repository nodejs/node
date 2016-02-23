The STORE type
==============

A STORE, as defined in this code section, is really a rather simple
thing which stores objects and per-object associations to a number
of attributes.  What attributes are supported entirely depends on
the particular implementation of a STORE.  It has some support for
generation of certain objects (for example, keys and CRLs).


Supported object types
----------------------

For now, the objects that are supported are the following:

X.509 certificate
X.509 CRL
private key
public key
number
arbitrary (application) data

The intention is that a STORE should be able to store everything
needed by an application that wants a cert/key store, as well as
the data a CA might need to store (this includes the serial number
counter, which explains the support for numbers).


Supported attribute types
-------------------------

For now, the following attributes are supported:

Friendly Name		- the value is a normal C string
Key ID			- the value is a 160 bit SHA1 hash
Issuer Key ID		- the value is a 160 bit SHA1 hash
Subject Key ID		- the value is a 160 bit SHA1 hash
Issuer/Serial Hash	- the value is a 160 bit SHA1 hash
Issuer			- the value is a X509_NAME
Serial			- the value is a BIGNUM
Subject			- the value is a X509_NAME
Certificate Hash	- the value is a 160 bit SHA1 hash
Email			- the value is a normal C string
Filename		- the value is a normal C string

It is expected that these attributes should be enough to support
the need from most, if not all, current applications.  Applications
that need to do certificate verification would typically use Subject
Key ID, Issuer/Serial Hash or Subject to look up issuer certificates.
S/MIME applications would typically use Email to look up recipient
and signer certificates.

There's added support for combined sets of attributes to search for,
with the special OR attribute.


Supported basic functionality
-----------------------------

The functions that are supported through the STORE type are these:

generate_object		- for example to generate keys and CRLs
get_object		- to look up one object
			  NOTE: this function is really rather
			  redundant and probably of lesser usage
			  than the list functions
store_object		- store an object and the attributes
			  associated with it
modify_object		- modify the attributes associated with
			  a specific object
revoke_object		- revoke an object
			  NOTE: this only marks an object as
			  invalid, it doesn't remove the object
			  from the database
delete_object		- remove an object from the database
list_object		- list objects associated with a given
			  set of attributes
			  NOTE: this is really four functions:
			  list_start, list_next, list_end and
			  list_endp
update_store		- update the internal data of the store
lock_store		- lock the store
unlock_store		- unlock the store

The list functions need some extra explanation: list_start is
used to set up a lookup.  That's where the attributes to use in
the search are set up.  It returns a search context.  list_next
returns the next object searched for.  list_end closes the search.
list_endp is used to check if we have reached the end.

A few words on the store functions as well: update_store is
typically used by a CA application to update the internal
structure of a database.  This may for example involve automatic
removal of expired certificates.  lock_store and unlock_store
are used for locking a store to allow exclusive writes.
