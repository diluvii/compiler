.text
.globl func
.type func, @function
func:
	pushl %ebp
	movl %esp, %ebp
	subl $20, %esp
	pushl %ebx
BB0:
	movl $1, -8(%ebp)
	movl $1, -16(%ebp)
	jmp BB2
BB1:
	movl -20(%ebp), %ebx
	movl %ebx, %eax
	popl %ebx
	leave
	ret
BB2:
	movl -8(%ebp), %ebx
	movl 8(%ebp), %ecx
	movl %ebx, %edx
	cmpl %ecx, %edx
	jl BB3
	jmp BB4
BB3:
	movl -8(%ebp), %ebx
	movl %ebx, %ecx
	addl $1, %ecx
	movl %ecx, -8(%ebp)
	movl -16(%ebp), %ecx
	imull %ebx, %ecx
	movl %ecx, -16(%ebp)
	jmp BB2
BB4:
	movl -16(%ebp), %ebx
	movl %ebx, -20(%ebp)
	jmp BB1
