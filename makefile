obj/red_blue: obj/red_blue.o
	mpicc -o $@ $^


obj/red_blue.o: RedBlue/red_blue_procedure.c
	mkdir obj
	mpicc -c -o $@ $<

clean:
	rm -r obj

