all: vec ls manager

vec: main.c monitor_neighbors.c
	gcc -pthread -o vec_router main.c monitor_neighbors.c

ls: main.c monitor_neighbors.c
	gcc -pthread -o ls_router main.c monitor_neighbors.c
manager: manager_send.c
	gcc -o manager manager_send.c

.PHONY: clean
clean:
	rm *.o vec_router ls_router manager
