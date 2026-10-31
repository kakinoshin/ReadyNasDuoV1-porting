#!/bin/sh
# Read `objdump -d` on stdin. V8 mul/div may appear only in the hw_* helpers.
awk '
/^[0-9a-f]+ <.*>:$/ { fn = $2 }
/[ \t](sdiv|sdivcc|udiv|udivcc|smul|smulcc|umul|umulcc)[ \t]/ {
	if (fn !~ /^<hw_/) { print "V8 insn outside hw_*: " fn " " $0; bad = 1 }
	else n++
}
END { print n + 0 " V8 mul/div insns, all inside hw_*"; exit bad }'
