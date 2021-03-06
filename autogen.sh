#!/bin/sh
#
# A simple script to add/reset autotools support
# This requires that autoconf and autoconf-archive are installed

run ()
{
    echo "Running: $*"
    eval $*

    if test $? != 0 ; then
	echo "Error: while running '$*'"
	exit 1
    fi
}

rm -rf autom4ate.cache m4
mkdir m4
run aclocal
run autoheader
LIBTOOLIZE=libtoolize
type $LIBTOOLIZE > /dev/null 2>&1 || LIBTOOLIZE=g$LIBTOOLIZE
run $LIBTOOLIZE --force --copy
run automake --add-missing --copy --force-missing --foreign
run autoconf
run autoreconf

rm -f man/*.usage man/*.1 man/*.1.html include/GeographicLib/Config-ac.h
