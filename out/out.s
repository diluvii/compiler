.text
.globl func
.type func, @function
func:
	pushl %ebp
	movl %esp, %ebp
	subl $28, %esp
	pushl %ebx
BB0:
	movl $1, -8(%ebp)
	movl $1, -12(%ebp)
	movl $0, -16(%ebp)
	br
BB1:
	movl $1, %eax
	popl %ebx
	leave
	ret
BB2:
	movl -16(%ebp), %ebx
	movl 8(%ebp), %ecx
	icmp
	br
BB3:
	movl -8(%ebp), %ebx
	call
	movl -16(%ebp), %ecx
	add/sub/mul
	movl %ecx, -16(%ebp)
	movl -12(%ebp), %ecx
	movl %ecx, -24(%ebp)
	add/sub/mul
	movl %ebx, -12(%ebp)
	movl -24(%ebp), %ebx
	movl %ebx, -8(%ebp)
	br
BB4:
	movl $1, -28(%ebp)
	br
