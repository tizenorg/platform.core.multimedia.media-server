#!/bin/sh

rm -rf autom4te.cache
rm -f aclocal.m4 ltmain.sh
mkdir -p m4

echo "Running aclocal..." ; aclocal $ACLOCAL_FLAGS -I m4 || exit 1
echo "Running autoheader..." ; autoheader || exit 1
echo "Running autoconf..." ; autoconf || exit 1
echo "Running libtoolize..." ; (libtoolize --copy --automake --force || glibtoolize --automake) || exit 1
echo "Running automake..." ; automake --add-missing --copy --foreign --force-missing || exit 1
echo "You can now compile and build package."
