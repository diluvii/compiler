.text
.globl func
.type func, @function
func:
	pushl %ebp
	movl %esp, %ebp
	subl $28, %esp
	pushl %ebx
BB0:
	store
	store
	store
	store
	br
BB1:
	return
BB2:
	load
	load
	icmp
	br
BB3:
	load
	call
	load
	add/sub/mul
	store
	load
	store
	add/sub/mul
	store
	load
	store
	br
BB4:
	store
	br
