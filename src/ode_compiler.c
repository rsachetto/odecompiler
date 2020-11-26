#include "parser.h"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "helpers.h"

int main(int argc, char **argv) {
   
	if(argc != 3) {
		fprintf(stderr, "Wrong number of arguments!\n");
		fprintf(stderr, "Usage: %s source_file outputfile\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	size_t file_size;	
	const char *source = read_entire_file_with_mmap(argv[1], &file_size);

	struct lexer l;
	init_lexer(&l, source);
    
    FILE *outfile = fopen(argv[2], "w");

    struct parser p;
	init_parser(&p, &l, outfile);

	program(&p);

}
