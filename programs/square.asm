
			lw		$a0,	$zero,	$imm,	256		# load start addr
			lw		$a1,	$zero,	$imm,	257		# load length	

			sra		$t0,	$a0,	$imm,	8		# $to is number of row
			mul		$t1,	$t0,	$imm,	256		# t1 = addr start square row
			add		$t2,	$t1,	$imm,	255		# t2 = addr end monitor row
			add		$t1,	$a1,	$a0,	 0		# t1 = start of square row + length + 1
			add		$t1,	$t1,	$imm,	-1		# t1 = end of square row (include)
			bgt		$imm,	$t1,	$t2,	end		# if end of square row > addr of last col -> END
			add		$t0,	$t0,	$a1,	$zero	# t0 = number of last row of square
			add		$t0,	$t0,	$imm,	-1		# t1 = end of square row (include)
			add		$t1,	$zero,	$imm,	255		# t1 = 255 - last row of monitor
			bgt		$imm,	$t0,	$t1,	end		# if start row index + length > number of last monitor row -> END
			add		$t0,	$zero,	$zero,	0		# i = 0
			add		$s0,	$zero,	$imm,	65536

blackloop: 
			beq		$imm,	$s0,	$t0,	preloop
			out		$t0,	$imm,	$zero,	20		# set address to draw
			add		$s2,	$imm,	$zero,	0
			out		$s2,	$imm,	$zero,	21		# set value to draw
			add		$t1,	$imm,	$zero,	1
			out		$t1,	$zero,	$imm,	22		# write to monitor
			add		$t0,	$t0,	$imm,	1		# i = i + 1
			beq		$imm,	$zero,	$zero,	blackloop
preloop:
			add		$t0,	$zero,	$zero,	0		# j = 0
			add		$t1,	$a0,	$zero,	0		# set cur_addr = a0
			add 	$t2,	$a0,    $a1,    0		#t2= end of square
loop:
			bge		$imm,	$t0,	$a1,	end		# if run over all the rows -> END
			blt		$imm,	$t1,	$t2,	white	# if cur_addr not bigger than addr of end square row -> WHITE
			add		$t2,	$t2,	$imm,	256		# change end_row addr to the end_row_addr of next row
			sub		$t1,	$t2,	$a1,	0
			add		$t0,	$t0,	$imm,	1
			beq		$imm,	$zero,	$zero,	loop	# next iteration on next row

white:
			out		$t1,	$imm,	$zero,	20		# set address to draw
			add		$s2,	$zero,	$imm,	255		
			out		$s2,	$imm,	$zero,	21		# set value to draw
			add		$s1,	$imm,	$zero,	1
			out		$s1,	$zero,	$imm,	22		# write to monitor
			add		$t1,	$t1,	$imm,	1		# move to next col	
			beq		$imm,	$zero,	$zero,	loop	# back to loop

end:		
			halt	$zero,	$zero,	$zero,	0

.word	256	0
.word	257	100