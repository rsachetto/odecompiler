#include "parser.h"

#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static char *read_entire_file_with_mmap(const char *filename, size_t *size) {

    char *f;

    if(!filename)
        return NULL;

    struct stat s;
    int fd = open(filename, O_RDONLY);

    if(fd == -1) {
        return NULL;
    }

    fstat(fd, &s);
    if(!S_ISREG(s.st_mode)) {
        close(fd);
        return NULL;
    }

    *size = s.st_size;

    size_t to_page_size = *size + 1;

    int pagesize = getpagesize();
    to_page_size += pagesize - (to_page_size % pagesize);

    f = (char *)mmap(0, to_page_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);

    if(f == NULL)
        return NULL;

    f[*size-1] = '\n'; //Add \n to ensure success in parsing
    f[*size] = '\0'; //Add \n to ensure success in parsing

    close(fd);

    return f;
}

int main(int argc, char **argv) {
   
	size_t file_size;	
	const char *source = read_entire_file_with_mmap(argv[1], &file_size);

	//TODO: new_lexer(source)
	struct lexer s;
    s.current_char = '\0';
    s.current_position = -1;
    s.source = source;
    next_char(&s);

    FILE *outfile = fopen(argv[2], "w");

    struct parser p;
	init_parser(&p, &s, outfile);

	program(&p);

}
