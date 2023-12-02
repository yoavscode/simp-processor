			lw		$t1,	$zero,		$imm,	500				# get sector number x from address 500
			out		$t1,	$zero,		$imm,	15            	#put sector number x in disksector
			add		$t1,	$zero,		$imm,	600				#t1= address to write to is 600
			out		$t1,	$zero,		$imm,	16 				#put address in buffer
			add		$t0,	$imm,		$zero,	1				#t0=1
			out		$t0,	$zero,		$imm,	14				#diskcmd=1 (read)
			add		$s0,	$zero,		$zero,	0				#s0=0 (s0 is the sum)
			add		$t0,	$imm,		$zero,	8				#t0=i=8

for:
			lw		$t2,	$zero,		$t1,	0				#t2= the word from t1(address)
			add		$s0,	$s0,		$t2,	0				#s0=s0+t2(sum)
			add		$t0,	$t0,		$imm,	-1				#t0=i=i-1
			add		$t1,	$imm,		$t1,	1 				#t1=t1+1 (next address)
			bne		$imm,	$t0,		$zero,	for				#while i!=0 -->for
		
			lw		$t1,	$zero,		$imm,	501				# get sector number y from address 501
			out		$t1,	$zero,		$imm,	15           	#put sector number y in disksector
			add		$t1,	zero,		$imm,	800				#t1= address to write to is 800
			out		$t1,	$zero,		$imm,	16 				#put address in buffer
		
busy:	
			in		$t0,	$zero,		$imm,	17				#get diskstatus
			bne		$imm,	$t0,		$zero,	busy			#wait until disk is free
		
			add		$t0,	$imm,		$zero,	1				#t0=1
			out		$t0,	$zero,		$imm,	14				#diskcmd=1 (read)
			add		$s1,	$zero,		$zero,	0				#s0=0 (s0 is the sum)
			add		$t0,	$imm,		$zero,	8				#t0=i=8
		
for2:
			lw		$t2,	$zero,		$t1,	0				#t2= the word from t1(address)
			add		$s1,	$s1,		$t2,	0				#s1=s1+t2(sum)
			add		$t0,	$t0,		$imm,	-1				#t0=i=i-1
			add		$t1,	$imm,		$t1,	1 				#t1=t1+1 (next address)
			bne		$imm,	$t0,		$zero,	for2			#while i!=0 -->for
		
			sw		$s0,	$imm,		$zero,	0X00100			#store sum in memory at 0x00100
			sw		$s1,	$imm,		$zero,	0X00101			#store sum in memory at 0x00101
		
			bgt		$imm,	$s1,		$s0,	s1big			#jump to s1big if s1>s0
			sw		$s0,	$imm,		$zero,	0X00102			#store s0 in memory at 0x00102
			halt	$zero,	$zero,		$zero,	0				# halt
		
s1big:
			sw		$s1,	$imm,		$zero,	0X00102			#store s1 in memory at 0x00102
			halt	$zero,	$zero,		$zero,	0				# halt

.word	500 100
.word  	501 99



		
		