#ifndef __HELPERS_H
#define __HELPERS_H 

static inline char *read_entire_file_with_mmap(const char *filename, size_t *size) {

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
	f[*size] = '\0'; 
	close(fd);

	return f;
}

#endif /* __HELPERS_H */
