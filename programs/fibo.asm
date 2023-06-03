		add		$s0,	$zero,	$imm,	0x01000			# $s0 =0x1000 (memory size=4096)
		add		$t0,	$zero,	$imm,	0x00100			# $t0 =0x0100
		add		$t1,	$zero,	$imm,	0				# $t1 = 0 - NO t1 = 1
		add		$t2,	$zero,	$imm,	1				# $t2 = 1
		sw		$t1,	$zero,	$t0,	0				#store t1 at 0X0100
		add		$t0,	$t0,	$imm,	1				#increse store address to 0x101
		sw		$t2,	$zero,	$t0,	0				#store t2 at 0x0101
		add		$t0,	$t0,	$imm,	1				#increse store address to 0x102
		add		$s1,	$s1,	$imm,	0x7ffff			#the last positive number

fib:	
		add		$t1,	$t1,	$t2,	0				# $t1=$t1+$t2
		sw		$t1,	$zero,	$t0,	0				#store t1 at t0's address
		add		$t0,	$t0,	$imm,	1				#increse t0 by one, (the next address)
		beq		$imm,	$s0,	$t0,	end				#if overflow- end program.
		add		$t2,	$t2,	$t1,	0				#$t2=$t2+$t1
		bge		$imm,	$t2,	$s1,	end				#break if reach negative number 
		sw		$t2,	$zero,	$t0,	0				#store t2 at t0 address
		add		$t0,	$t0,	$imm,	1				#increse t0 by one, (the next address)
		bne		$imm,	$s0,	$t0,	fib				#keep calculating until t0=4096	

end:
		halt	$zero,	$zero,	$zero,	0				# halt


