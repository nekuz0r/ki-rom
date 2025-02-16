# Credits to : LZ77 Decode Demo by krom (Peter Lemon)

.text
.set noreorder
.set nomacro
.set noat

.align 8
.globl lzss_decompress
lzss_decompress: 
    lbu $t0,0x3($a0)
    lbu $t1,0x2($a0)
    sll $t0,0x8
    or $t0,$t1
    lbu $t1,1($a0)
    sll $t0,0x8
    or $t0,$t1
    addu $t0,$a1
    addiu $a0,0x4

    1$:
        lbu $t1,0x0($a0)
        addiu $a0,0x1
        ori $t2,$zero,0x80
        2$:
            beq $a1,$t0,5$
            and $t4,$t1,$t2
            beqz $t2,1$
            srl $t2,0x1
            lbu $t3,0x0($a0)
            bnez $t4,3$
            addiu $a0,0x1
            sb $t3,0x0($a1)
            j 2$
            addiu $a1,0x1
        3$:
            lbu $t4,0x0($a0)
            addiu $a0,0x1
            sll $t5,$t3,0x8
            or $t4,$t5
            andi $t4,0xfff
            nor $t4,$zero
            addu $t4,$a1
            srl $t3,0x4
            addiu $t3,0x3
            4$:
                lbu $t5,0x0($t4)
                addiu $t4,0x1
                sb $t5,0x0($a1)
                addiu $t3,-0x1
                bnez $t3,4$
                addiu $a1,0x1
                j 2$
                nop
    5$:

    jr $ra
    or $v0,$zero,$a1
    