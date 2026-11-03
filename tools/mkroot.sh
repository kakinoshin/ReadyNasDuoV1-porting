#!/bin/sh
# Build the test init and the FFS image that goes into the kernel as md0, with
# the crunched /rescue binaries from the sparc distribution as the userland.
# Usage: tools/mkroot.sh OUTPUT
set -e

out=$1
top=$(cd "$(dirname "$0")/.." && pwd)
tools=$top/src/netbsd-tools
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

mkdir -p "$work/root/sbin" "$work/root/dev" "$work/root/tmp"
cp -a "$top/src/netbsd-obj/destdir.sparc/rescue" "$work/root/rescue"
"$tools/bin/sparc--netbsdelf-gcc" -O2 -mcpu=v7 -mno-fpu -ffreestanding \
    -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static -Wall \
    -Wl,--build-id=none -o "$work/root/sbin/init" \
    "$top/hw/init/start.S" "$top/hw/init/init.c"
"$tools/bin/sparc--netbsdelf-objdump" -d "$work/root/sbin/init" |
    sh "$top/hw/probe/check-v7.sh"

cat > "$work/spec" <<SPEC
/set type=dir uid=0 gid=0 mode=0755
.
dev
console type=char device=netbsd,4,0 uid=0 gid=0 mode=0600
null type=char device=netbsd,2,2 uid=0 gid=0 mode=0666
sd0a type=block device=netbsd,2,0 uid=0 gid=0 mode=0640
rsd0a type=char device=netbsd,6,0 uid=0 gid=0 mode=0640
sd0b type=block device=netbsd,2,1 uid=0 gid=0 mode=0640
rsd0b type=char device=netbsd,6,1 uid=0 gid=0 mode=0640
sd0c type=block device=netbsd,2,2 uid=0 gid=0 mode=0640
rsd0c type=char device=netbsd,6,2 uid=0 gid=0 mode=0640
sd0d type=block device=netbsd,2,3 uid=0 gid=0 mode=0640
rsd0d type=char device=netbsd,6,3 uid=0 gid=0 mode=0640
sd0e type=block device=netbsd,2,4 uid=0 gid=0 mode=0640
rsd0e type=char device=netbsd,6,4 uid=0 gid=0 mode=0640
sd0f type=block device=netbsd,2,5 uid=0 gid=0 mode=0640
rsd0f type=char device=netbsd,6,5 uid=0 gid=0 mode=0640
sd0g type=block device=netbsd,2,6 uid=0 gid=0 mode=0640
rsd0g type=char device=netbsd,6,6 uid=0 gid=0 mode=0640
sd0h type=block device=netbsd,2,7 uid=0 gid=0 mode=0640
rsd0h type=char device=netbsd,6,7 uid=0 gid=0 mode=0640
..
sbin
init type=file uid=0 gid=0 mode=0555
..
tmp mode=01777
..
..
SPEC

"$tools/bin/nbmakefs" -t ffs -B be -s 15m -F "$work/spec" "$out" "$work/root"
