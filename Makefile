all:
	mpicc training_mpi.c -o training_mpi
	mpicc match_mpi.c -o match_mpi
training:
	mpirun -np 12 ./training_mpi > training.lab.o
match:
	mpirun -np 34 ./match_mpi > match.lab.o
clean:
	rm training_mpi match_mpi