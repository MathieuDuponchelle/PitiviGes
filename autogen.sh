#!/bin/sh 
# Run this to generate all the initial makefiles, etc.

DIE=0
package=Gnonlin
srcfile=gnl/gnlsource.h

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $package."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/autoconf/"
	DIE=1
}

(automake --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have automake installed to compile $package."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/automake/"
	DIE=1
}
automakevermin=`(automake --version|head -n 1|sed 's/^.* //;s/\./ /g;';echo "1 4")|sort -n|head -n 1`
automakevergood=`(automake --version|head -n 1|sed 's/^.* //;s/\./ /g;';echo "1 4f")|sort -n|head -n 1`
if test "x$automakevermin" != "x1 4"; then
# version is less than 1.4, the minimum suitable version
	echo
	echo "You must have automake version 1.4 or greater installed."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at ftp://ftp.gnu.org/pub/gnu/automake/"
	DIE=1
fi


(pkg-config --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have pkg-config installed to compile $package."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at:"
	echo "http://www.freedesktop.org/software/pkgconfig/"
	DIE=1
}


(libtool --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have libtool installed to compile $package."
	echo "Get the latest version from ftp://alpha.gnu.org/gnu/libtool/"
	DIE=1
}

libtool_version=`libtool --version | sed 's/^.* \([0-9\.]*\) .*$/\1/'`
libtool_major=`echo $libtool_version | cut -d. -f1`
libtool_minor=`echo $libtool_version | cut -d. -f2`
libtool_micro=`echo $libtool_version | cut -d. -f3`
if [ x$libtool_micro = x ]; then
	libtool_micro=0
fi
if [ $libtool_major -le 1 ]; then
	if [ $libtool_major -lt 1 ]; then
		echo
		echo "You must have libtool 1.3.5 or greater to compile $package."
		echo "Get the latest version from ftp://alpha.gnu.org/gnu/libtool/"
		DIE=1
	elif [ $libtool_minor -le 3 ]; then
		if [ $libtool_minor -lt 3 ]; then
			echo
			echo "You must have libtool 1.3.5 or greater to compile $package."
			echo "Get the latest version from ftp://alpha.gnu.org/gnu/libtool/"
			DIE=1
		elif [ $libtool_micro -lt 5 ]; then
			echo
			echo "You must have libtool 1.3.5 or greater to compile $package."
			echo "Get the latest version from ftp://alpha.gnu.org/gnu/libtool/"
			DIE=1
		fi
	fi
fi

if test "$DIE" -eq 1; then
	exit 1
fi

test -f $srcfile || {
	echo "You must run this script in the top-level $package directory"
	exit 1
}

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi


# Generate configure.in and configure.ac
./makeconfigure <configure.base > configure.in configure.in
./makeconfigure <configure.base > configure.ac configure.ac

libtoolize --copy --force
aclocal $ACLOCAL_FLAGS || {
	echo
	echo "aclocal failed - check that all needed development files are present on system"
	exit 1
}
autoheader || {
	echo
	echo "autoheader failed"
	exit 1
}
autoconf || {
	echo
	echo "autoconf failed"
	#exit 1
}
automake --add-missing || {
	echo
	echo "automake failed"
	#exit 1
}

# now remove the cache, because it can be considered dangerous in this case
rm -f config.cache

# The new configure options for busy application developers (Hadess)
#./configure --enable-maintainer-mode --enable-debug --enable-debug-verbose 

./configure --enable-maintainer-mode --enable-plugin-builddir --enable-debug --enable-DEBUG "$@" || {
	echo
	echo "configure failed"
	exit 1
}

echo 
echo "Now type 'make' to compile $package."
