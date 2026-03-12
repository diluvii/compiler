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
	jmp BB2
BB1:
	movl $1, %eax
	popl %ebx
	leave
	ret
BB2:
	movl -16(%ebp), %ebx
	movl 8(%ebp), %ecx
	movl %ebx, %edx
	cmpl %ecx, %edx
	jl BB3
	jmp BB4
BB3:
	movl -8(%ebp), %ebx
	pushl %ecx
	pushl %edx
	pushl %ebx
	call print
	addl $4, %esp
	popl %edx
	popl %ecx
	movl -16(%ebp), %ecx
	addl $1, %ecx
	movl %ecx, -16(%ebp)
	movl -12(%ebp), %ecx
	movl %ecx, -24(%ebp)
	addl %ecx, %ebx
	movl %ebx, -12(%ebp)
	movl -24(%ebp), %ebx
	movl %ebx, -8(%ebp)
	jmp BB2
BB4:
	movl $1, -28(%ebp)
	jmp BB1
