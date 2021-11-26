#!/bin/bash
# 
# Total clean-up for distribution.
make distclean 2>/dev/null || true
rm -fv configure config.log aclocal.m4 Makefile.in Makefile
rm -Rfv config autom4te.cache m4

