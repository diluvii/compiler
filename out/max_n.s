.text
.globl func
.type func, @function
func:
	pushl %ebp
	movl %esp, %ebp
	subl $24, %esp
	pushl %ebx
BB0:
	movl $0, -16(%ebp)
	movl $0, -12(%ebp)
	jmp BB2
BB1:
	movl -24(%ebp), %ebx
	movl %ebx, %eax
	popl %ebx
	leave
	ret
BB2:
	movl -12(%ebp), %ebx
	movl 8(%ebp), %ecx
	movl %ebx, %edx
	cmpl %ecx, %edx
	jl BB3
	jmp BB4
BB3:
	pushl %ecx
	pushl %edx
	call read
	popl %edx
	popl %ecx
	movl %eax, %ebx
	movl %ebx, -8(%ebp)
	movl -8(%ebp), %ebx
	movl -16(%ebp), %ecx
	movl %ebx, %edx
	cmpl %ecx, %edx
	jg BB5
	jmp BB6
BB4:
	movl -16(%ebp), %ebx
	movl %ebx, -24(%ebp)
	jmp BB1
BB5:
	movl -8(%ebp), %ebx
	movl %ebx, -16(%ebp)
	jmp BB6
BB6:
	movl -12(%ebp), %ebx
	addl $1, %ebx
	movl %ebx, -12(%ebp)
	jmp BB2
