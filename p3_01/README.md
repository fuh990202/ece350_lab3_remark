## ECE 350 Lab 3 Folder

This folder contains the University of Waterloo ECE 350 Real-Time Opearting Systems Lab3 materials for Group Number 1.

## File structures
### `code`
This folder contains the source code we implemented for this lab.

### `code/svc/src/INC`
This folder contains header files for the RTX APIs.

### `code/svc/src/app`
This folder contains sample third-party test cases.

### `code/svc/app/ae_mem.c`
This file contains all the test cases we wrote for testing our functions for inter-task communication with interrupts.

### `code/svc/app/ae_priv_tasks.c`
This file contains all the generic test cases for testing our functions for inter-task communication with interrupts.

### `code/svc/app/ae_priv_tasks.h`
This file contains all the generic test cases headers for testing our functions for inter-task communication with interrupts.

### `code/svc/app/ae_usr_tasks.c`
This file contains all the test cases we wrote for testing our functions for inter-task communication with interrupts.

### `code/svc/app/ae_usr_tasks.h`
This file contains all the function headers we wrote for testing our functions for inter-task communication with interrupts.

### `code/svc/app/kcd_tasks.c`
This file contains all the KCD related test cases we wrote for testing our functions for inter-task communication with interrupts.

### `code/svc/app/kcd_tasks.h`
This file contains all the KCD related test cases we wrote for testing our functions for inter-task communication with interrupts.

### `code/svc/src/board/VE_A9`
This folder contains the board support package for the ARM VE A9 fixed virtual platform.

### `code/svc/src/board/DE1_SoC_A9`
This folder contains the board support package for the ARM DE1 SoC A9 real hardware board.

### `code/svc/src/kernel`
This folder contains all the kernel source code.

### `code/svc/src/kernel/k_mem.c`
This file contains the functionalities of `mem_init`, `mem_alloc`, `mem_dealloc`,`k_alloc_k_stack`,`k_alloc_p_stack` and two utility functions: `mem_count_extfrag` and `get_free_memory`(for testing purpose only).

### `code/svc/src/kernel/k_inc.h`
This file contains the structure of tcb with extra information fields.

### `code/svc/src/kernel/k_mem.c`
This file contains the functionalities of `mem_init`, `mem_alloc`, `mem_dealloc`,`k_alloc_k_stack`,`k_alloc_p_stack` and two utility functions: `mem_count_extfrag` and `get_free_memory`(for testing purpose only).

### `code/svc/src/kernel/k_msg.c`
This file contains the functionalities of `k_mbx_create`, `k_send_msg`, `k_recv_msg`,`k_mbx_ls`,`get_real_size`.

### `code/svc/src/kernel/k_rtx_init.c`
This file contains the generic functionalities of `k_rtx_init`, `k_rtx_init_rt` for initialization of inter-task communication.

### `code/svc/src/kernel/k_task.c`
This file contains the functionalities of `tsk_init`, `tsk_create_new`, `tsk_switch`, `tsk_run_new`, `tsk_yield`, `tsk_create`, `tsk_exit`, `tsk_set_prio`, `tsk_get`, `scheduler`.

### `code/svc/src/kernel/main_svc_cw.c`
This file contains the main function file for our own testing purpose implementation.

### `p3_report.pdf`
This file is the lab report for lab 3.
