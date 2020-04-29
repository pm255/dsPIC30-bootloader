#! /bin/bash
set -e

SRCDIR="gld-original"
DSTDIR="gld-modified"

BOOTLOADER_SIZE=0x800

PLENREG="^ *program *\(xr\) *: *ORIGIN *= *0x100 *, *LENGTH *= *(.*) *$"

for srcfile in $SRCDIR/*.gld
do
	dstfile=$DSTDIR/$(basename $srcfile)

	[[ `grep -m 1 -i -P "$PLENREG" $srcfile` =~ $PLENREG ]]
	bootloader_addr=$(printf 0x%X $((${BASH_REMATCH[1]}+0x100-$BOOTLOADER_SIZE)))

	echo $dstfile $bootloader_addr

	awk -f gld-modifier.awk BOOTLOADER_ADDR=$bootloader_addr $srcfile > $dstfile
done
