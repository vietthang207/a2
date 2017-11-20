all:
	mpicc training_mpi.c -o training_mpi
	mpicc match_mpi.c -o match_mpi
training:
	mpirun -np 12 ./training_mpi > training.lab.o
match:
	mpirun -np 34 ./match_mpi > match.lab.o
clean:
	rm training_mpi match_mpi
run:
	for i in 1 2 3 4 5 6 7 8 ; do\
		echo "run with $$((i)) cores"; \
		mpirun -machinefile machinefile.1 -rankfile rankfile.$$i -np 34 ./match_mpi > match.lab.1 ;\
		echo ; \
	done