BEGIN {
	mask = 0
}

/^OPTIONAL\(-lfx\)$/ {
	print
	print ""
	printf "_BOOTLOADER_BASE_ADDRESS = 0x%X;\n", BOOTLOADER_ADDR
	print "_BOOTLOADER_SIZE = 0x800;"
	mask = mask + "0x0001"
	next
}

/^  data  \(a!xr\)   : ORIGIN = 0x800,         LENGTH = (.*)$/ {
	print "/*", $0, "*/"
	print "  data  (a!xr)   : ORIGIN = 0x800,         LENGTH = 0x100"
	mask = mask + "0x0002"
	next
}

/^  reset          : ORIGIN = 0x0,           LENGTH = 0x4$/ {
	print "/*", $0, "*/"
	print "  reset          : ORIGIN = 0x0,           LENGTH = 0x40"
	mask = mask + "0x0004"
	next
}

/^  ivt            : ORIGIN = 0x4,           LENGTH = 0x7C$/ {
	print "/*", $0, "*/"
	mask = mask + "0x0008"
	next
}

/^  \} >reset$/ {
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x08);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x08 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x0C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x0C >> 16);"
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x10);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x10 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x14);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x14 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x18);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x18 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x1C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x1C >> 16);"
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x20);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x20 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x24);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x24 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x28);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x28 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x2C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x2C >> 16);"
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x30);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x30 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x34);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x34 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x38);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x38 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x3C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x3C >> 16);"
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x40);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x40 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x44);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x44 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x48);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x48 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x4C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x4C >> 16);"
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x50);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x50 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x54);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x54 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x58);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x58 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x5C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x5C >> 16);"
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x60);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x60 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x64);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x64 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x68);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x68 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x6C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x6C >> 16);"
	print ""
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x70);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x70 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x74);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x74 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x78);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x78 >> 16);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x7C);"
	print "        SHORT(_BOOTLOADER_BASE_ADDRESS + 0x7C >> 16);"
	print ""
	print $0
	mask = mask + "0x0010"
	next
}

/^  \.text :$/ {
	print "/*", $0, "*/"
	print "  .text _BOOTLOADER_BASE_ADDRESS :"
	mask = mask + "0x0020"
	next
}

/^        \*\(\.init\);$/ {
	print "        FILL(0xFFFFFFFF);"
	print "        . = . + 0x80; /* jump table */"
	print $0
	mask = mask + "0x0040"
	next
}

/^        \*\(\.lib\*\);$/ {
	print $0
	print "        *(.text);"
	print "        __CHECK = ASSERT(. <= (_BOOTLOADER_BASE_ADDRESS + _BOOTLOADER_SIZE), \"The size of the bootloader code is too big\");"
	mask = mask + "0x0080"
	next
}

{
	print
}

END {
	if (mask != +"0x00FF") {
		exit 1
	}
}