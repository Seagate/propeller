# Propeller Source Code Documentation

## Initialization
```mermaid
sequenceDiagram
main.c ->> main.c: ilm_read_args()
Note right of main.c: Read arguments for STX lock manager
main.c ->> main.c: ilm_daemon_setup()
Note right of main.c: Daemonize program
main.c ->> log.c: ilm_log_init()
Note over main.c, log.c: Spin up logging thread and open log file
main.c ->> main.c: ilm_signal_setup()
Note right of main.c: Set up signal handling for ILM shutdown
main.c ->> drive.c: ilm_scsi_list_init()
Note over main.c, drive.c: Fill drive list with attached SCSI drives and initialize thread for drive
main.c ->> client.c: ilm_client_listener_init()
Note over main.c, client.c: Generate lock file and setup socket listener to handle clients
main.c ->> cmd.c: ilm_cmd_queue_create()
Note over main.c, cmd.c: Initialize command list and associated pthread mutexes/conditions, spinning up thread for commands sent to ILM
main.c ->> main.c: ilm_main_loop()
Note right of main.c: Infinite while loop for handling client requests
```
## Main Loop
```mermaid
sequenceDiagram
Loop Until Shutdown
  main.c ->> client.c: ilm_client_is_updated()
  client.c ->> main.c: ret
  alt ret is 1
    main.c ->> client.c: ilm_client_alloc_pollfd()
  end
  main.c ->> client.c: ilm_client_handle_request()
 end
```
## client.c/client.h
This handles client requests by setting up a listener on start-up. Clients that connect are added to a global list of client structs.
https://github.com/brandonohare/propeller/blob/32c06a6a51914ce00e2f38b7120d7d301d6a221c/src/client.h#L18-L27
Once the clients are part of the global list, their states can updated and requests they send can be handled and results sent back to the correct file descriptors.

We start off by adding a client to the list, with a function to be handled: ilm_client_connect().
```mermaid
sequenceDiagram
ilm_client_listener_init() ->> ilm_client_add(): listener_sock_fd, ilm_client_connect(), NULL
ilm_client_add() ->> list_add(): client
```
[The main loop](#main-loop), which is checking if the client list is updated, notices the update. It polls the file descriptor and handles the connection. 
Connecting to the client adds the client back to the list to handle their following request after establishing connection. 
```mermaid
sequenceDiagram
ilm_client_connect() ->> ilm_client_add(): client_fd, ilm_client_request, NULL
ilm_client_add() ->> list_add(): client
```
After the [main loop](#main-loop) handles this second request, it uses ilm_client_request() to recv a command from the client and add it to the command queue.
```mermaid
sequenceDiagram
client.c-ilm_client_handle_request() ->> client.c-ilm_client_request(): client
client.c-ilm_client_request() ->> cmd.c-ilm_cmd_queue_add_work: client
```
## cmd.c/cmd.h

## drive.c/drive.h

## failure.c/failure.c

## idm_pthread_backend.c

## idm_scsi.c

## idm_wrapper.h

## ilm.h

## ilm_internal.h

## inject_fault.c/inject_fault.h

## lib_client.c
This file provides the API for lvmlockd to use the IDM lock manager. This allows lvmlockd to connect to the socket, create lockspaces, check locks, etc.
## libseagate_ilm.pc
This is the package config file, which assists LVM in checking if the IDM lock manager is installed before configuring LVM to use it.
## list.h
This file is based on the standard Linux linked-list structure and is used by files such as `client.c` to create a client list. 
## lock.c/lock.h

## lockspace.c/lockspace.h

## log.c/log.h

## logrotate.ilm

## main.c

## raid_lock.c/raidlock.h

## scsiutils.c/scsiutils.h

## seagate_ilm.service

## util.c/util.h

## uuid.c/uuid.h
