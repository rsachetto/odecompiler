#include <stdio.h>
#include <stdlib.h>

#include "compiler/parser.h"
#include "compiler/helpers.h"

#define STB_DS_IMPLEMENTATION
#include "code_converter.h"
#include "stb/stb_ds.h"

int main(int argc, char **argv) {

	if(argc != 3) {
		fprintf(stderr, "Wrong number of arguments!\n");
		fprintf(stderr, "Usage: %s source_file output_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	size_t file_size;
	const char *source = read_entire_file_with_mmap(argv[1], &file_size);

	lexer *l = new_lexer(source, argv[1]);
	parser *p = new_parser(l);
	program program = parse_program(p);

	check_parser_errors(p);	
	
	FILE *outfile = fopen(argv[2], "w");
	convert_to_c(program, outfile);	
	fclose(outfile);

}
