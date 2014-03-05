UCOSDIR=${X32_PACKAGE_ROOT}/ucos 
COMPORT=${X32_PACKAGE_SERIAL}

default: cc
.PHONY : clean default cc cc-x32 test

cc: cc.ce
	x32-upload cc.ce -c ${COMPORT}
	x32-term start

cc.ce: cc.c monitor.c
	lcc-x32 -g cc.c monitor.c -I${UCOSDIR} -Wl-lib -Wl. -Wl-lib -Wl${UCOSDIR} -o cc.ce 

clean: 
	rm cc.ce cc.dbg 
