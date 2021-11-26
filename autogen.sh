#!/bin/bash
rm -Rfv m4
mkdir m4
autoreconf --force --install -I config -I m4
rm -Rf autom4te.cache
rm -Rf m4

