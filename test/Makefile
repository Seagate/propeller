all:
	gcc -I../src -ggdb -o smoke_test smoke_test.c -L../src -lseagate_ilm -luuid
	gcc -I../src -ggdb -o killsignal_test killsignal_test.c -L../src -lseagate_ilm -luuid

clean:
	rm -f smoke_test killsignal_test
