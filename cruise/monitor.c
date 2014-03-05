#include <ucos.h>

/*----------------------------------------------------------------
 * General defs
 *----------------------------------------------------------------
 */

#define	SIGNON		"\r\nMonitor (c) A.J.C. van Gemund, Mark Dufour, Sijmen Woutersen\r\n"
#define	MENU		"\r\nCommand Menu:\r\n\
h                              print this menu\r\n\
d [addr1 [addr2]]              dump memory\r\n\
e  addr1 [data | addr2 data]   edit memory\r\n\
s  addr1 [addr2]               sense memory\r\n\
\n"
//i                              print int etc sizes\r\n\
//q                              exit monitor\r\n\
//x                              exit monitor\r\n\
//r  addr                        run code\r\n\
//t  [ms]                        print/reset time or test delay function\r\n\

char    ARROW_0 = 0x1b;
char    ARROW_1 = 0x5b;
char    ARROW_UP = 0x41;
char    ARROW_DN = 0x42;

#define MAXLEN  16
#define MAXHIST 16

#define POKE(addr,val)  (*(unsigned char*) (addr) = (val))
#define PEEK(addr)      (*(unsigned char*) (addr))

char        string[MAXLEN];
char        history[MAXHIST][MAXLEN];
int     hist_last, hist_size;
char        command[MAXLEN];
int     argc;
#define MAXARGC 4
char        *argv[MAXARGC];

unsigned int    dump_start;
char        message[100];

int	cmd_run(void);
int	cmd_dump(void);
int	cmd_help(void);
int	cmd_int(void);
int	cmd_set(void);
int	cmd_exit(void);

void mon_puts(char *s) {
    printf("%s", s);
    //puts(s);
}

void    mon_putchar(char c) {
    putchar(c);
}

char mon_getchar() {
    return getchar();
}

char mon_getchar_nb() {
    return getchar_nb();
}

/*----------------------------------------------------------------
 * mon_strlen -- return length of '\0'-terminated string
 *----------------------------------------------------------------
 */
int mon_strlen(char *str)
{
    int i = 0;

    while (*str++ != '\0') i++;
    return i;
}

/*----------------------------------------------------------------
 * mon_strcpy -- copy strings
 *----------------------------------------------------------------
 */
char    *mon_strcpy(char *dest, char *src)
{
    while (*src != '\0') *dest++ = *src++;
    *dest = '\0';
    return dest;
}

/*----------------------------------------------------------------
 * mon_strequal -- return true if strings are equal
 *----------------------------------------------------------------
 */
int mon_strequal(char *str1, char *str2)
{
    while (*str1 != '\0' && *str2 != '\0')
        if (*str1++ != *str2++)
            return 0;
    return *str1 == *str2;
}

void	mon_delay_ms(unsigned int delta) 
{
    OSTimeDly(5); // fix later

}

/*----------------------------------------------------------------
 * hex2nibble -- convert hex char to nibble 0 .. 15, -1 on error
 *----------------------------------------------------------------
 */
int hex2nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}

/*----------------------------------------------------------------
 * hex2ui -- convert hex string to unsigned int, ret -1 on error
 *----------------------------------------------------------------
 */
int hex2ui(char *hex, int bytesize, unsigned int *result)
{
    int     i, j, v;
    unsigned int    max;

    max = 1;
    for (i = 0; i < bytesize * 4 - 1; i++)
        max = (max << 1) | 1;
    *result = 0;
    i = 0;
    j = mon_strlen(hex);
    for (i = 0; i < j; i++) {
        v = hex2nibble(hex[i]);
        if (v == -1) {
            *result = 0;
            mon_puts("[hex2ui format error]\r\n");
            return -1;
        }
        /* check if + v fits within bytesize
         */
        if ((*result + v) > max) {
            *result = 0;
            mon_puts("[hex2ui ext overflow]\r\n");
            return -1;
        }
        *result = *result + v;
        if (i < j-1) {

            /* check if << fits within sizeof(ui)
             */
            if ((*result * 16) / 16 != *result) {
                *result = 0;
                mon_puts("[hex2ui int overflow]\r\n");
                return -1;
            }
            /* check if << fits within bytesize
             */
            if ((*result * 16) > max) {
                *result = 0;
                mon_puts("[hex2ui ext overflow]\r\n");
                return -1;
            }

            *result = *result * 16;
        }
    }

    return 0;
}

/*----------------------------------------------------------------
 * nibble2hex -- return hex char of nibble value 0 .. 15
 *----------------------------------------------------------------
 */
char    nibble2hex(int n)
{
    if (n < 10)
        return n + '0';
    if (n < 16)
        return n + 'A' - 10;
    mon_puts("[nibble2hex overflow]\r\n");
    return '-';
}

/*----------------------------------------------------------------
 * ui2hex -- write unsigned int c in hex format
 *----------------------------------------------------------------
 */
void    ui2hex(unsigned int c, int digits, char *hex)
{
    int i, j;
    char    s[10];

    if (digits == -1) { // free format
        i = 0;
        do {
            s[i] = nibble2hex(c % 16);
            c = c / 16;
            i++;
        } while (c > 0);
        for (j = 0; j < i; j++)
            hex[j] = s[i-j-1];
        hex[i] = '\0';
    }
    else { // fixed # digits leading zeros

            for (i = digits-1; i >= 0; i--) {
                    hex[i] = nibble2hex(c % 16);
                    c = c / 16;
            }
            if (c == 0)
                    hex[digits] = '\0';
            else
                    mon_strcpy(hex,"[ui2hex overflow]");
    }
}

/*----------------------------------------------------------------
 * nibble2dec -- return dec char of nibble value 0 .. 9
 *----------------------------------------------------------------
 */
char	nibble2dec(int n)
{
	if (n < 10) 
		return n + '0';
	mon_strcpy(message,"[nibble2dec overflow]\r\n");
	return '-';
}

/*----------------------------------------------------------------
 * ui2dec -- write c in decimal format 
 *----------------------------------------------------------------
 */
void	ui2dec(unsigned int c, int digits, char *dec)
{
	int	i, j;
	char	s[10];

	if (digits == -1) { // free format
		i = 0;
		do {
			s[i] = nibble2dec(c % 10);
			c = c / 10;
			i++;
		} while (c > 0);
		for (j = 0; j < i; j++)
			dec[j] = s[i-j-1];
		dec[i] = '\0';
	}
	else { // fixed # digits leading zeros

        	for (i = digits-1; i >= 0; i--) {
                	dec[i] = nibble2dec(c % 10);
                	c = c / 10;
        	}
        	if (c == 0)
                	dec[digits] = '\0';
        	else
                	mon_strcpy(dec,"[ui2dec overflow]");
	}
}

/*----------------------------------------------------------------
 * do_ctl_h -- backspace 1 char 
 *----------------------------------------------------------------
 */
void	do_ctl_h(void)
{
	mon_putchar(0x08);
	mon_putchar(' ');
	mon_putchar(0x08);
}

/*----------------------------------------------------------------
 * do_ctl_u -- clear line of i chars
 *----------------------------------------------------------------
 */
void	do_ctl_u(int i)
{
	int	j;

	if (i > 0) {
		for (j = 0; j < i; j++) {
			do_ctl_h();
		}
	}
}

/*----------------------------------------------------------------
 * hexdump -- dump memory in hex
 *        next address (!= end if aborted)
 *        return -1 if aborted
 *----------------------------------------------------------------
 */
int hexdump(unsigned int start, unsigned int end, unsigned int *next)
{
    unsigned int    N, addr;
    unsigned char   data;
    int     i, j;
    char        c;

    N = end - start;
    N = N & 0xfffff; /* wrap around 1MB */
    // printf("start = %04x, end = %04x, N = %04x\n",start,end,N);

    /* print data
     */
    for (i = 0; i < (N / 16) + 1; i++) {
        if (i*16 >= N)
            break;
        addr = start+i*16 & 0xfffff;
        *next = start+(i+1)*16 & 0xfffff;
        ui2hex(addr,5,message);
        mon_puts(message);
        mon_puts("  ");
        for (j = 0; j < 16; j++) {
            addr = start+i*16+j & 0xfffff;
            if (i*16+j < N) {
                data = PEEK(addr);
                ui2hex(data,2,message);
                mon_puts(message);
                mon_puts(" ");
            }
            else {
                mon_puts("   ");
            }
        }
        mon_puts("   ");
        for (j = 0; j < 16; j++) {
            addr = start+i*16+j & 0xfffff;
            if (i*16+j < N) {
                data = PEEK(addr);
                if ((data >= 0x20) &&
                        (data < 0x80)) {
                    mon_putchar(data);
                }
                else
                    mon_putchar('.');
            }
        }
        mon_puts("\r\n");

        /* if key pressed pause or exit
         */
        if ((c = mon_getchar_nb()) == -1)
            continue;

        return -1;

    }
    return 0;

}

/*----------------------------------------------------------------
 * getline -- simple line input
 *----------------------------------------------------------------
 */
int	getline(char *string, char *prompt, int echo)
{
  	int	c;
  	int	i;
	int	hist_count, hist_index;

	/* clear buffer just in case of funny chars 
	 */
	/*for (i = 0; i < 10; i++) {
		mon_delay_ms(1);
		mon_getchar_nb();
	}*/

	/* start history at last entered command
	 */
	hist_index = hist_last;
	hist_count = 0;

	/* prompt and read line
	 */
	mon_puts(prompt);
	i = 0;
	for (;;) {

		c = mon_getchar();

		/* go down in history
		 */
		if (c == ARROW_0) {
			c = mon_getchar();
			if (c == ARROW_1) {
				c = mon_getchar();

				if (c == ARROW_UP) {

					if (hist_count == hist_size)
						continue;

					/* else process ARROW_UP
					 */
					hist_count++;

					/* ctl_u ; echo command
					 */
					do_ctl_u(i);
					i = 0;
					hist_index--;
					if (hist_index < 0)
						hist_index = MAXHIST-1;
					mon_strcpy(string,history[hist_index]);
					mon_puts(string);
					i = mon_strlen(string);
					continue;
				}

				if (c == ARROW_DN) {

					if (hist_count == 0)
						continue;

					/* else process ARROW_DN
					 */
					hist_count--;

					/* ctl_u ; echo command except 
					 * count == 0
					 */
					do_ctl_u(i);
					i = 0;
					hist_index++;
					if (hist_index == MAXHIST)
						hist_index = 0;
					if (hist_count == 0)
						continue;
					mon_strcpy(string,history[hist_index]);
					mon_puts(string);
					i = mon_strlen(string);
					continue;
				}

				continue;
			}
			else {
				continue;
			}
		}

		/* check for line formatting
		 */
		if (c == 0x08) { // ^H
			if (i > 0) {
				do_ctl_h();
				i--;
			}
			continue;
		}
		if (c == 0x15) { // ^U
			do_ctl_u(i);
			i = 0;
			continue;
		}

		/* ^C is immediate exit
		 */
		if (c == 0x03) { // ^C
			if (echo)
				mon_puts("\r\n");
			cmd_exit();
		} 

		/* other char goes into string
		 */
		if (c == '\r' || c == '\n')
			break;
		if (echo)
			mon_putchar(c);
		string[i++] = c;
		if (i == MAXLEN-1)
			break;
	}

	/* input entered, update buffer if no "" and return input
	 */
	if (echo)
		mon_puts("\r\n");
	string[i] = '\0';
	if (i > 0) {
		mon_strcpy(history[hist_last],string);
		hist_last++; if (hist_last == MAXHIST) hist_last = 0;
		if (hist_size < MAXHIST) hist_size++;
	}
  	return i;
}

/*----------------------------------------------------------------
 * getargs -- parse command line in argc / argv format
 *----------------------------------------------------------------
 */
int     getargs(char *cmd, char *argv[])
{
        int             argc;
        int             c,l;

        l = mon_strlen(cmd);
        c = 0;
        for (argc = 0; c < l; argc++) {
        if (argc == MAXARGC)
            return argc;
                while (c < l && (cmd[c] == ' ' || cmd[c] == '\t'))
                        c++;
                if (c == l)
                        break;
                argv[argc] = &(cmd[c]);
                while (c < l && (cmd[c] != ' ' && cmd[c] != '\t'))
                        c++;
                cmd[c] = '\0';
                c++;
        }
        return argc;
}

/*----------------------------------------------------------------
 * cmd_dump -- dump memory contents
 *----------------------------------------------------------------
 */
int cmd_dump(void)
{
    unsigned int    result, start, end;

    if (argc == 1) {
        start = dump_start;
        end = start + 0x100;
        hexdump(start,end,&dump_start);
        return 0;
    }

    result = hex2ui(argv[1],5,&start);
    if (result == -1) {
        return -1;
    }
    if (argc == 2) {
        end = start + 0x100;
    }
    else {
        result = hex2ui(argv[2],5,&end);
        if (result == -1) {
            return -1;
        }
    }
    hexdump(start,end,&dump_start); 
    return 0;
}

/*----------------------------------------------------------------
 * cmd_exit -- exit monitor
 *----------------------------------------------------------------
 */
int	cmd_exit(void)
{
	mon_puts("<exit>\r\n");
	//mon_exitio();
	exit(0);
    return 0;
}

/*----------------------------------------------------------------
 * cmd_help -- print command menu
 *----------------------------------------------------------------
 */
int	cmd_help(void)
{
	mon_puts(MENU);
	return 0;
}

/*----------------------------------------------------------------
 * cmd_int -- print int etc sizes
 *----------------------------------------------------------------
 */
int	cmd_int(void)
{
	mon_puts("sizeof(int) = ");
	ui2dec(sizeof(int),-1,message);
	mon_puts(message);
	mon_puts("\r\n");

	mon_puts("sizeof(unsigned int) = ");
	ui2dec(sizeof(unsigned int),-1,message);
	mon_puts(message);
	mon_puts("\r\n");

	mon_puts("sizeof(long) = ");
	ui2dec(sizeof(long),-1,message);
	mon_puts(message);
	mon_puts("\r\n");

	mon_puts("sizeof(unsigned long) = ");
	ui2dec(sizeof(unsigned long),-1,message);
	mon_puts(message);
	mon_puts("\r\n");
	return 0;
}

/*----------------------------------------------------------------
 * cmd_set -- edit memory contents
 *----------------------------------------------------------------
 */
int	cmd_set(void)
{
	unsigned int	result, start, end, addr, data, i, N;

	if (argc == 1) {
		mon_puts("no args\r\n");
		return -1;
	}
	result = hex2ui(argv[1],5,&start);
	if (result == -1) {
		return -1;
	}

	/* single data arg
	 */
	if (argc == 3) {
		result = hex2ui(argv[2],2,&data);
		if (result == -1) {
			return -1;
		}
		POKE(start,data);
		if (PEEK(start) != data) {
			mon_puts("warning: memory verification: ");
			ui2hex(PEEK(start),2,message);
			mon_puts(message);
			mon_puts("\r\n");
		}
		return 0;
	}

	/* double addr + data arg
	 */
	if (argc == 4) {
		result = hex2ui(argv[2],5,&end);
		if (result == -1) {
			return -1;
		}
		result = hex2ui(argv[3],2,&data);
		if (result == -1) {
			return -1;
		}
		N = (end - start) & 0xfffff;
		// printf("start = %04x, end = %04x, N = %04x\n",start,end,N);
		for (i = 0; i < N; i++) {
			addr = (start + i) & 0xfffff;
			POKE(addr,data);
		}
		return 0;
	}

	/* two data arg: go interactive
	 */
	addr = start;
	for (;;) {
		ui2hex(addr,5,message);
		mon_puts(message);
		mon_puts(" ");
		data = PEEK(addr);
		ui2hex(data,2,message);
		mon_puts(message);
		mon_puts(" ");

		getline(message,"",1);
		if (mon_strlen(message) == 0) {
			addr = (addr + 1) & 0xfffff;
			continue;
		}
		if (mon_strequal(message,".")) {
			continue;
		}
		if (mon_strequal(message,"-")) {
			addr = (addr - 1) & 0xfffff;
			continue;
		}
		if (mon_strequal(message,"x")) {
			return 0;
		}

		result = hex2ui(message,2,&data);
		if (result == -1) {
			mon_puts("data format error\r\n");
			return -1;
		}
		POKE(addr,data);
		if (PEEK(addr) != data) {
			mon_puts("warning: memory verification: ");
			ui2hex(PEEK(start),2,message);
			mon_puts(message);
			mon_puts("\r\n");
		}
		addr = (addr + 1) & 0xfffff;
	}
}

/*----------------------------------------------------------------
 * cmd_sense -- read memory contents continuously
 *----------------------------------------------------------------
 */
int	cmd_sense(void)
{
	unsigned int	result, start, end, next;

	if (argc == 1) {
		mon_puts("no args");
		return -1;
	}

	result = hex2ui(argv[1],5,&start);
	if (result == -1) {
		return -1;
	}
	if (argc == 2) {
		end = (start + 1) & 0xfffff;
	}
	else {
		result = hex2ui(argv[2],5,&end);
		if (result == -1) {
			return -1;
		}
	}

	for (;;) {
		if (hexdump(start,end,&next) != 0)
			break;
		mon_delay_ms(100);
	}
	return 0;
}

/*----------------------------------------------------------------
 * execute -- execute command
 *----------------------------------------------------------------
 */
void	execute(char *string)
{
	mon_strcpy(command,string);
	argc = getargs(command,argv);
	if (argc == 0)
		return;

	/* parse command, exec function
	 */
	if (mon_strequal(argv[0],"d")) {
		if (cmd_dump()) cmd_help();
		return;
	}
	if (mon_strequal(argv[0],"e")) {
		if (cmd_set()) cmd_help();
		return;
	} 
	if (mon_strequal(argv[0],"h")) {
		cmd_help();
		return;
	}
	/*if (mon_strequal(argv[0],"i")) {
		cmd_int();
		return;
	} */
	/*if (mon_strequal(argv[0],"m")) {
		if (cmd_math()) cmd_help();
		return;
	} 
	if (mon_strequal(argv[0],"q")) {
		cmd_exit();
		return;
	} 
	if (mon_strequal(argv[0],"r")) {
		if (cmd_run()) cmd_help();
		return;
	} */ 
	if (mon_strequal(argv[0],"s")) {
		if (cmd_sense()) cmd_help();
		return;
	} 
	/*if (mon_strequal(argv[0],"t")) {
		if (cmd_test()) cmd_help();
		return;
	}
	if (mon_strequal(argv[0],"x")) {
		cmd_exit();
		return;
	}  */
	mon_puts("illegal command\r\n");
	cmd_help();
}


/*----------------------------------------------------------------
 * main -- execute monitor
 *----------------------------------------------------------------
 */
int run_monitor()
{
	for (hist_last = 0; hist_last < MAXHIST; hist_last++) {
		history[hist_last][0] = '\0';
	}
	hist_last = 0;
	hist_size = 0;
	dump_start = 0;

	//mon_initio();
	mon_puts(SIGNON);
	mon_puts(MENU);

	for (;;) {
		getline(string,"mymon > ", 1);
		execute(string);
	}

}
