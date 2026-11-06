#!/bin/sh
# Build the padre kernel and hw/kmod/padre_go.ko around it.
# Usage: tools/build-kernel.sh [KERNCONF]
#
# build.sh does not know MACHINE=padre yet, so this runs config(1) and make
# directly with the sparc tools. The result is a flat image that padre_go.ko
# enters at its first byte.
set -e

conf=${1:-PADRE}
top=$(cd "$(dirname "$0")/.." && pwd)
src=$top/src/netbsd-src
tools=$top/src/netbsd-tools
obj=$top/src/netbsd-obj/sys/arch/padre/compile/$conf

mkdir -p "$obj"
(cd "$src/sys/arch/padre/conf" && "$tools/bin/nbconfig" -b "$obj" -s "$src/sys" "$conf")

# nbmake-sparc with the target machine changed.
sed 's/^MACHINE=sparc;/MACHINE=padre;/' "$tools/bin/nbmake-sparc" > "$obj/nbmake-padre"
(cd "$obj" && MAKEOBJDIR= sh ./nbmake-padre -j4 dependall)

"$tools/bin/sparc--netbsdelf-objdump" -d "$obj/netbsd" | sh "$top/hw/probe/check-v7.sh"
if grep -q '^options[[:space:]]*MEMORY_DISK_IS_ROOT' "$src/sys/arch/padre/conf/$conf"; then
	"$top/tools/mkroot.sh" "$obj/root.fs"
	"$tools/bin/sparc--netbsdelf-mdsetimage" "$obj/netbsd" "$obj/root.fs"
fi
"$tools/bin/sparc--netbsdelf-objcopy" -O binary "$obj/netbsd" "$obj/netbsd.bin"

make -C "$top/hw/kmod" IMAGE="$obj/netbsd.bin" clean all
