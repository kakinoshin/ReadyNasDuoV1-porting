#!/bin/sh
# Build a freestanding sparc-elf cross toolchain (binutils + GCC C only, libgcc)
# for IT3107 bare-metal work: boot stubs, hardware probes, kernel experiments.
#
# Default code generation is SPARC V7. The IT3107 advertises V8 but has a
# signed-divide erratum, so V8 div instructions must not appear by default.
#
# Needs no root: m4 is built locally, GMP/MPFR/MPC/ISL are built in-tree.
#
# Usage: sh build-sparc-elf.sh [jobs]     (default 2; GCC needs ~1 GB per job)
set -eu

JOBS=${1:-2}
BINUTILS=binutils-2.47
GCC=gcc-16.2.0
M4=m4-1.4.20
TARGET=sparc-elf

TOP=$(cd "$(dirname "$0")" && pwd)
DL=$TOP/dl
SRC=$TOP/build/src
OBJ=$TOP/build/obj
PREFIX=$TOP/sparc-elf
HOSTTOOLS=$TOP/host
GNU=https://ftp.jaist.ac.jp/pub/GNU

mkdir -p "$DL" "$SRC" "$OBJ" "$PREFIX" "$HOSTTOOLS"
export PATH="$PREFIX/bin:$HOSTTOOLS/bin:$PATH"

fetch() { # url
	f=$DL/$(basename "$1")
	[ -s "$f" ] || curl -fL --retry 5 -o "$f.part" "$1" && { [ -s "$f" ] || mv "$f.part" "$f"; }
}

unpack() { # tarball, dirname
	[ -d "$SRC/$2" ] || tar -xf "$DL/$1" -C "$SRC"
}

fetch $GNU/m4/$M4.tar.xz
fetch $GNU/binutils/$BINUTILS.tar.xz
fetch $GNU/gcc/$GCC/$GCC.tar.xz

unpack $M4.tar.xz $M4
unpack $BINUTILS.tar.xz $BINUTILS
unpack $GCC.tar.xz $GCC

if [ ! -x "$HOSTTOOLS/bin/m4" ]; then
	mkdir -p "$OBJ/m4" && cd "$OBJ/m4"
	"$SRC/$M4/configure" --prefix="$HOSTTOOLS"
	make -j"$JOBS" MAKEINFO=true && make install MAKEINFO=true
fi

if [ ! -x "$PREFIX/bin/$TARGET-ld" ]; then
	mkdir -p "$OBJ/binutils" && cd "$OBJ/binutils"
	"$SRC/$BINUTILS/configure" --target=$TARGET --prefix="$PREFIX" \
	    --disable-nls --disable-werror --disable-gdb --disable-sim
	make -j"$JOBS" MAKEINFO=true && make install MAKEINFO=true
fi

if [ ! -d "$SRC/$GCC/gmp" ]; then
	(cd "$SRC/$GCC" && ./contrib/download_prerequisites)
fi

if [ ! -x "$PREFIX/bin/$TARGET-gcc" ]; then
	mkdir -p "$OBJ/gcc" && cd "$OBJ/gcc"
	"$SRC/$GCC/configure" --target=$TARGET --prefix="$PREFIX" \
	    --with-cpu=v7 --enable-languages=c --without-headers --with-newlib \
	    --disable-nls --disable-shared --disable-threads --disable-libssp \
	    --disable-libquadmath --disable-libgomp --disable-libatomic
	make -j"$JOBS" all-gcc all-target-libgcc MAKEINFO=true
	make install-gcc install-target-libgcc MAKEINFO=true
fi

"$PREFIX/bin/$TARGET-gcc" --version | head -1
"$PREFIX/bin/$TARGET-ld" --version | head -1
