all:
	gcc -I../src -ggdb -o smoke_test smoke_test.c -L../src -lseagate_ilm -luuid
	gcc -I../src -ggdb -o killsignal_test killsignal_test.c -L../src -lseagate_ilm -luuid
	gcc -I../src -ggdb -o killpath_test killpath_test.c -L../src -lseagate_ilm -luuid
	gcc -I../src -ggdb -o killpath_notifier killpath_notifier.c -L../src -lseagate_ilm -luuid
	gcc -I../src -ggdb -o stress_test stress_test.c -L../src -lseagate_ilm -luuid -lpthread

clean:
	rm -f smoke_test killsignal_test killpath_test killpath_notifier
