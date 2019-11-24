#include "mipsdbg.h"

#include "mipssim.h"

#include <stdlib.h>
#include <stdio.h>


#define in_err(chr) { if(!chr) {err_msg(); return; } }

void *_db_cache = NULL;

int step = 0;
int init = 1;
int breaks[90];
int no_breaks= 0;
int every_clock = 0;

void err_msg(){
	printf("Error parsing the input!\n");
}


void move_index(char *buf, size_t *i, char t){
	while (buf[*i] != '\0' && buf[*i] != t ){
		(*i)++;
	}
}

void gdb_parse_param(struct architectural_state* arch, char *param){
	size_t i = 0;
	int val;
	char s = param[i];
	switch(s){
		case 'a':
			move_index(param, &i, '.'); i++;
			switch(param[i]){
				case 's': // state
					printf("State: %d\n", arch->state);
					break;
				case 'r': // register
					move_index(param, &i, '['); in_err(param[i]); i++;
					sscanf(param+i, "%d", &val);
					printf("Register $%d = %0.8x (%d)\n",
							val, arch->registers[val], arch->registers[val]);
					break;
				case 'm': // memory
					move_index(param, &i, '['); in_err(param[i]); i++;
					sscanf(param+i, "%d", &val);
					printf("memory[%d] = %0.8x (%d)\n",
							val, arch->memory[val], arch->memory[val]);
					break;
				case 'c': // control
					move_index(param, &i, '['); in_err(param[i]); i++;
					sscanf(param+i, "%d", &val);
					printf("control[%d] = %d\n",
							val, ((int*)&arch->control)[val]);
					break;
				case 'p': // curr_pipe_regs
					move_index(param, &i, '['); in_err(param[i]); i++;
					sscanf(param+i, "%d", &val);
					printf("pipe_reg[%d] = %0.8x (%d)\n",
							val, ((int*)&arch->curr_pipe_regs)[val], ((int*)&arch->curr_pipe_regs)[val]);
					break;
			}
			break;
		case 'b':
			printf("Breakpoints:\n");
			for (int i=0; i < no_breaks; i++){
				if (breaks[i] != -1){
					printf("%d: PC = %d\n", i, breaks[i]);
				}
			}
			break;
		case 'c':
			printf("Loads:\t %ld\nHits:\t %ld\nStores:\t %ld\nHits:\t %ld\n",
				arch->mem_stats.lw_total,
				arch->mem_stats.lw_cache_hits,
				arch->mem_stats.sw_total,
				arch->mem_stats.sw_cache_hits);
			break;
		default:
			err_msg();
	}

}

void print_help(){
	printf("%s",
            "Commands:\n"
            "     c, continue\n"
            "        Continues execution of the program untill a breakpoint is found or program exits\n"
            "     s, n, next\n"
            "        Executes the curent line\n"
            "     b, break [i]\n"
            "        Puts a breakpoint at PC=i\n"
            "     d, delete [i]\n"
            "        Deletes breakpoint i (loop at print breakpoints)\n"
            "     h, help\n"
            "        Prints this message\n"
            "     H, HELP\n"
            "        Prints mapping of control and pipe to names in code\n"
            "     p, i, print [what]\n"
            "        Prints value described in what:\n"
            "        b, breakpoints - prints breakpoints\n"
            "        a, arch_state:\n"
            "            r, register [i]\n"
            "            m, memory [i] - Word at i<<2\n"
            "            s, state - current fsm state\n"
            "            p, pipe [i] - pipe register i (look at H)\n"
            "            c, control [i] - control i (look at H)\n"
			"     x [i]\n"
			"        Prints string in memory at address i<<2\n"
			"        I do not believe you will ever have a reason to use this,\n"
			"        but if you ever want to print some text you can look at the code if this command\n"
            "\n"
            "Example Usage:\n"
            "    b 2\n    c\n    p a.r[3]\n    n\n    p a.r[3]\n"
            "        If memfile-complex.txt is used this set of comands \n"
			"        will go to the 2nd line by setting a breakpoint there\n"
			"        and print 3rd register before and after the addi $3, $0, 3072\n"
            "Notes:\n"
            "    To use the a.s a.p a.c on every cycle of the processor change EVERY_CLOCK define to 1, \n"
			"    otherwise those might prove useless\n"
            );
}

void more_help(){
	printf("%s",
"Control bindings:\n"
"     0: RegDst\n"
"     1: RegWrite\n"
"     2: ALUSrcA\n"
"     3: MemRead\n"
"     4: MemWrite\n"
"     5: MemtoReg\n"
"     6: IorD\n"
"     7: IRWrite\n"
"     8: PCWrite\n"
"     9: PCWriteCond\n"
"    10: ALUOp\n"
"    11: ALUSrcB\n"
"    12: PCSource\n"
"\n"
"Pipe regs bindings\n"
"    0: pc\n"
"    1: IR\n"
"    2: A\n"
"    3: B\n"
"    4: ALUOut\n"
"    5: MDR\n"
);

}

void __gdb_loop(struct architectural_state* arch, int *running){
	char *buffer = NULL;
	size_t buff_size=0;
	int val= 0;
	getline(&buffer, &buff_size, stdin);
	size_t index = 0;
	switch (buffer[index]){
		case 'c': // continue
			*running = 0;
			break;
		case 'n': // there is no call so next and step are the same
		case 's':
			step = 1;
			*running = 0;
			break;
		case 'i':
		case 'p': // info and print were meged
			move_index(buffer, &index , ' '); in_err(buffer[index]); index ++;
			gdb_parse_param(arch, buffer + index);
			break;
		case 'b':
			move_index(buffer, &index, ' '); in_err(buffer[index]); index++;
			sscanf(buffer+index, "%d", &val);
			breaks[no_breaks] = val;
			no_breaks++;
			printf("Added breakpoint at pc == %d\n", val);
			break;
		case 'd':
			move_index(buffer, &index, ' '); in_err(buffer[index]); index++;
			sscanf(buffer+index, "%d", &val);
			printf("Removed breakpoint at PC = %d\n", breaks[val]);
			breaks[val] = -1;
			break;
		case 'x':
			move_index(buffer, &index, ' '); in_err(buffer[index]); index++;
			sscanf(buffer+index, "%d", &val);
			printf("%s\n", (char *) (arch->memory+val));
			break;
		case 'h':
			print_help();
			break;
		case 'H':
			more_help();
			break;
		default:
			err_msg();

	}

	free(buffer);
	buffer = NULL;
}

void __gdb_run(struct architectural_state* arch){
	int running = 1;
	while (running) {
		__gdb_loop(arch, &running);
	}
}

void __gdb(struct architectural_state* arch){
	if (arch->state == EXIT_STATE){
		__gdb_run(arch);
		return;
	}
	if (arch->state != DECODE && !EVERY_CLOCK){
		return;
	}
	for (int i=0 ; i < no_breaks;i++){
		if (arch->curr_pipe_regs.pc/4-1 == breaks[i]){
			__gdb_run(arch);
			return;
		}
	}
	if (init){
		init = 0;
		__gdb_run(arch);
	} else if (step){
		step = 0;
		__gdb_run(arch);
	}
}
