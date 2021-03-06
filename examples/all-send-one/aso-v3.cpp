#include "mpi.h"

int main (int argc, char *argv[]) {
  enum { TAG_A };

  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (rank == 0) {
    int ns[size-1];
    MPI_Request requests[size-1];
    for (int i=0, src=1; src < size; i++, src++) {
      MPI_Irecv(&ns[i], 1, MPI_INT, src, TAG_A, MPI_COMM_WORLD, &requests[i]);
    }
    MPI_Waitall(size-1, requests, MPI_STATUSES_IGNORE);
  } else {
    MPI_Send(&rank, 1, MPI_INT, 0, TAG_A, MPI_COMM_WORLD);
  }

  MPI_Finalize();
  return 0;
}
