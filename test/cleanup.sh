#! /bin/sh

dump_mutex() {
	sg_raw -v -r 512 /dev/$1              88 00 01 00 00 00 00 20 FF 01 00 00 00 01 00 00
}

destroy_mutex() {
	sg_raw -v -r 512 -o remov.bin /dev/$1 88 00 01 00 00 00 00 20 FF 01 00 00 00 01 00 00
	sg_raw -s 512 -i remov.bin /dev/$1    89 00 01 00 00 00 00 20 FF 01 00 00 00 01 05 00
	sg_raw -s 512 -i remov.bin /dev/$1    89 00 01 00 00 00 00 20 FF 01 00 00 00 01 07 00
}


destroy_mutex sg2
destroy_mutex sg3
destroy_mutex sg4
destroy_mutex sg5
destroy_mutex sg6
destroy_mutex sg7
destroy_mutex sg8
destroy_mutex sg9

dump_mutex sg2
dump_mutex sg3
dump_mutex sg4
dump_mutex sg5
dump_mutex sg6
dump_mutex sg7
dump_mutex sg8
dump_mutex sg9
