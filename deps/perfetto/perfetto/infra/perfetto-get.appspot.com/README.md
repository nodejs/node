This folder contains the sources for the
https://get.perfetto.dev/XXX web service.

This service simply proxies request of the form

https://get.perfetto.dev/XXX

to the raw content of the files hosted on AOSP Gerrit, e.g.:

https://android.googlesource.com/platform/external/perfetto/+/master/XXX

See main.py for the dictionary that maps /XXX into the actual path in the
perfetto repository.
