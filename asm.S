#
# PROJECT:		FreeLoader wrapper for Apple TV
# LICENSE:      MIT (https://spdx.org/licenses/MIT)
# PURPOSE:		Assembly functions for FreeLoader wrapper for original Apple TV
# COPYRIGHT:	Copyright 2023-2024 DistroHopper39B (distrohopper39b.business@gmail.com)
#

.extern _printf
.extern _WrapperInit

.text
.globl start
.globl _fail

start:
    # Push BootArgs pointer to C loader
    pushl %eax
    # Jump to C
	call _WrapperInit
	# Halt the system
	hlt

_fail:
    # Print error to the screen.
    push $msg_halted
    call _printf
    # Halt the CPU
    hlt

.data
    msg_halted: .ascii "FATAL: Could not load Linux! System halted.\n\0"