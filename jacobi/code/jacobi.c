#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

void save_gnuplot(double *M, size_t dim);

void evolve(double *matrix, double *matrix_new, size_t myRows, size_t dimension);

int above_peer(int myRank);
int below_peer(int myRank, int nProcesses);

int main(int argc, char *argv[]) {
  int myRank, nProcesses;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
  MPI_Comm_size(MPI_COMM_WORLD, &nProcesses);

  int aboveRank = above_peer(myRank);
  int belowRank = below_peer(myRank, nProcesses);

  size_t dimension = 0, iterations = 0, row_peek = 0, col_peek = 0;
  size_t byte_dimension = 0;

  if (argc != 3) {
    if (myRank == 0) {
      fprintf(stderr, "\nwrong number of arguments. Usage: ./a.out dim it\n");
    }

    MPI_Finalize();
    return 1;
  }

  dimension = atoi(argv[1]);
  iterations = atoi(argv[2]);

  if (myRank == 0) {
    printf("matrix size = %zu\n", dimension);
    printf("number of iterations = %zu\n", iterations);
  }

  size_t myRows = dimension / nProcesses;
  myRows += myRank < dimension % nProcesses;

  double *matrix, *matrix_new;

  byte_dimension = sizeof(double) * (myRows + 2) * (dimension + 2);
  matrix = (double *)malloc(byte_dimension);
  matrix_new = (double *)malloc(byte_dimension);

  memset(matrix, 0, byte_dimension);
  memset(matrix_new, 0, byte_dimension);

  // initial values
  for (size_t i = 1; i <= myRows; ++i)
    for (size_t j = 1; j <= dimension; ++j)
      matrix[(i * (dimension + 2)) + j] = 0.5;

  // borders
  double increment = 100.0 / (dimension + 1);

  double incrementStart = increment * dimension * (1 - (double) myRank / nProcesses);
  if (myRank > dimension % nProcesses) {
    incrementStart += dimension % nProcesses;
  } else {
    incrementStart += myRank;
  }
  for (size_t i = 1; i <= myRows + 1; ++i) {
    matrix[i * (dimension + 2)] = i * increment + incrementStart;
    matrix_new[i * (dimension + 2)] = i * increment + incrementStart;
  }

  for (size_t i = 1; i <= dimension + 1; ++i) {
    matrix[((myRows + 1) * (dimension + 2)) + (dimension + 1 - i)] =
        i * increment;
    matrix_new[((myRows + 1) * (dimension + 2)) + (dimension + 1 - i)] =
        i * increment;
  }

  int rowSize = 1 + dimension + 1;

  int recvTopIdx = 1;
  int sendTopIdx = recvTopIdx + rowSize;

  int sendBottomIdx = (myRows - 2) * rowSize + 1;
  int recvBottomIdx = sendBottomIdx + rowSize;

  MPI_Barrier(MPI_COMM_WORLD);
  double t_start = MPI_Wtime();
  for (size_t it = 0; it < iterations; ++it) {
    evolve(matrix, matrix_new, myRows, dimension);

    double *tmp_matrix = matrix;
    matrix = matrix_new;
    matrix_new = tmp_matrix;

    MPI_Sendrecv(matrix + sendTopIdx, dimension, MPI_DOUBLE, aboveRank, 0,
                 matrix + recvBottomIdx, dimension, MPI_DOUBLE, belowRank, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Sendrecv(matrix + sendBottomIdx, dimension, MPI_DOUBLE, belowRank, 0,
                 matrix + recvTopIdx, dimension, MPI_DOUBLE, aboveRank, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
  }
  MPI_Barrier(MPI_COMM_WORLD);
  double t_end = MPI_Wtime();

  if (myRank == 0)
    printf("\nelapsed time = %f seconds\n", t_end - t_start);

  save_gnuplot(matrix, dimension);

  free(matrix);
  free(matrix_new);

  MPI_Finalize();

  return 0;
}

int above_peer(int myRank) {
  if (myRank == 0) {
    return MPI_PROC_NULL;
  }
  return myRank - 1;
}

int below_peer(int myRank, int nProcesses) {
  if (myRank == nProcesses - 1) {
    return MPI_PROC_NULL;
  }
  return myRank + 1;
}

void evolve(double *matrix, double *matrix_new, size_t myRows, size_t dimension) {
  for (size_t i = 1; i <= myRows; ++i)
    for (size_t j = 1; j <= dimension; ++j)
      matrix_new[(i * (dimension + 2)) + j] =
          (0.25) * (matrix[((i - 1) * (dimension + 2)) + j] +
                    matrix[(i * (dimension + 2)) + (j + 1)] +
                    matrix[((i + 1) * (dimension + 2)) + j] +
                    matrix[(i * (dimension + 2)) + (j - 1)]);
}

const double h = 0.1;
void save_gnuplot(double *M, size_t dimension) {
  FILE *file;

  file = fopen("solution.dat", "w");

  for (size_t i = 0; i < dimension + 2; ++i)
    for (size_t j = 0; j < dimension + 2; ++j)
      fprintf(file, "%f\t%f\t%f\n", h * j, -h * i,
              M[(i * (dimension + 2)) + j]);

  fclose(file);
}
