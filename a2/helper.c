#include "my_alloc.c"

int main(int argc, char *argv[]){
	my_init();
	char* test = NULL;
	if (test == NULL){
		printf("null pointer\n");
	}else{
		printf("allocated\n");
	}
	test = my_alloc(8);
	*(test+0) = 'a';
	*(test+1) = 'b';
	*(test+2) = 'c';
	*(test+3) = 'd';
	printf("before free: %s\n", test);
	my_free(test);
	printf("after free: %s\n", test);
	my_heapinfo();
	my_clean();
}
