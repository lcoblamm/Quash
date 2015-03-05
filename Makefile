quash: quash.c
	gcc -g -O0 quash.c -o quash

clean:
	rm -r quash *~ *.dSYM
