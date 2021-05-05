#include <stdio.h>
#include <stdlib.h>

#include "compiler/parser.h"
#include "compiler/helpers.h"

#include "code_converter.h"

#include "file_utils/file_utils.h"

int main(int argc, char **argv) {

	if(argc != 3) {
		fprintf(stderr, "Error: wrong number of arguments!\n");
		fprintf(stderr, "Usage: %s source_file output_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    char *file_name = argv[1];

    if(!file_exists(file_name)) {
    	fprintf(stderr, "Error: file %s does not exist!\n", file_name);
		exit(EXIT_FAILURE);
    }


	size_t file_size;
	const char *source = read_entire_file_with_mmap(file_name, &file_size);

	lexer *l = new_lexer(source, file_name);
	parser *p = new_parser(l);
	program program = parse_program(p);

	check_parser_errors(p);	
	
	FILE *outfile = fopen(argv[2], "w");
	convert_to_c(program, outfile);	
	fclose(outfile);

}
