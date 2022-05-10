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

## libseagate_ilm.pc

## list.h

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
