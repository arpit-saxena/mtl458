debug:
	gcc test.c -DDEBUG -Wpedantic -o test -g

release:
	gcc test.c -Wpedantic -o test -g

FOLDER_NAME=2018MT10742_A2

submit:
	mkdir -p ${FOLDER_NAME}
	cp -f my_alloc.c comments.txt ${FOLDER_NAME}
	zip -r ${FOLDER_NAME}.zip ${FOLDER_NAME}
	rm -r ${FOLDER_NAME}