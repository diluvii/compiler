.text
.globl func
.type func, @function
func:
	pushl %ebp
	movl %esp, %ebp
	subl $16, %esp
	pushl %ebx
BB0:
	movl 8(%ebp), %ebx
	movl %ebx, -12(%ebp)
	jmp BB2
BB1:
	movl -16(%ebp), %ebx
	movl %ebx, %eax
	popl %ebx
	leave
	ret
BB2:
	movl -12(%ebp), %ebx
	movl %ebx, %ecx
	cmpl $1, %ecx
	jg BB3
	jmp BB4
BB3:
	movl -12(%ebp), %ebx
	subl $2, %ebx
	movl %ebx, -12(%ebp)
	jmp BB2
BB4:
	movl -12(%ebp), %ebx
	movl %ebx, -16(%ebp)
	jmp BB1
