#!/bin/sh
# Build a root file system for padre from the sparc distribution.
# Usage: tools/mkusbroot.sh OUTPUT		FFS image for the USB disk
#        tools/mkusbroot.sh -n SERVER:PATH OUTPUT	tar.gz for an NFS root
#
# The kernel mounts the image as sd0c, the whole disk, with no label. The
# tarball carries owners and device nodes, so it is extracted as root on the
# server; the kernel configures the interface for an NFS root, so dhcpcd is
# left off there. Manual pages, tests and headers are left out to keep the
# root small. Device nodes come from majors.padre, not from the sparc MAKEDEV.
set -e

nfs=
if [ "$1" = -n ]; then
	nfs=$2
	shift 2
fi
out=$1
top=$(cd "$(dirname "$0")/.." && pwd)
tools=$top/src/netbsd-tools
dest=$top/src/netbsd-obj/destdir.sparc
work=$(mktemp -d "$top/src/netbsd-obj/usbroot.XXXXXX")
trap 'rm -rf "$work"' EXIT

grep -vE '^\./(usr/share/man|usr/share/doc|usr/tests|usr/include|dev/)' \
    "$dest/METALOG" | grep -vE '^\./(etc/(fstab|rc\.conf|ttys))( |$)' \
    > "$work/spec"

# A tree of hard links, so that the changed files do not touch the destdir.
cp -al "$dest" "$work/root"
rm -f "$work/root/etc/fstab" "$work/root/etc/ttys" "$work/root/etc/rc.conf"
if [ -n "$nfs" ]; then
	echo "$nfs	/	nfs	rw	0 0" > "$work/root/etc/fstab"
	dhcpcd=NO
else
	echo "/dev/sd0c	/	ffs	rw	1 1" > "$work/root/etc/fstab"
	dhcpcd=YES
fi
cat > "$work/root/etc/ttys" <<TTYS
console	"/usr/libexec/getty std.9600"	vt100	on secure
TTYS
sed 's/^rc_configured=NO/rc_configured=YES/' "$dest/etc/rc.conf" \
    > "$work/root/etc/rc.conf"
cat >> "$work/root/etc/rc.conf" <<RC
hostname=padre
no_swap=YES
savecore=NO
cron=NO
postfix=NO
sshd=NO
dhcpcd=$dhcpcd
RC

{
	for f in fstab ttys rc.conf; do
		echo "./etc/$f type=file uname=root gname=wheel mode=0644"
	done
	echo "./dev type=dir uname=root gname=wheel mode=0755"
	node() {
		echo "./dev/$1 type=$2 device=netbsd,$3,$4 uname=root gname=$5 mode=$6"
	}
	node console char 4 0 wheel 0600
	node tty char 1 0 wheel 0666
	node mem char 2 0 kmem 0640
	node kmem char 2 1 kmem 0640
	node null char 2 2 wheel 0666
	node zero char 2 12 wheel 0666
	node drum char 3 0 kmem 0640
	node bpf char 10 0 wheel 0600
	i=0
	for p in a b c d e f g h; do
		node sd0$p block 2 $i operator 0640
		node rsd0$p char 6 $i operator 0640
		i=$((i + 1))
	done
} >> "$work/spec"

if [ -n "$nfs" ]; then
	(cd "$work/root" && "$tools/bin/nbpax" -dOw -z -N "$dest/etc" -M \
	    < "$work/spec") > "$out"
else
	"$tools/bin/nbmakefs" -t ffs -B be -s 900m -N "$dest/etc" \
	    -F "$work/spec" "$out" "$work/root"
fi
