/*
Result:

Area=7583461.801486

Tasks Per Process
0       1       2       3       4
0       39      195     1059    5271

The quad function is copied from aquadsequential.c with some adjustment.
It is now able to return two values, the sub-area value and recursion times.

Same as PartA, I defined some const to make the code more clear,such as FARMER,RIGHT,AREA,TIME.

For farmer:
It is much simpler than the farmer in partA.
Just calculate the the size of a chunk and the start and end points of each chunk.
Then distribute the chunk to every process.
After that, receive the sub-area values and recursion times by using "wild cards"(MPI_ANY_SOURCE and MPI_ANY_TAG).
Figure out the received message belongs to which process by status.MPI_SOURCE.
Add the sub-area value to the final result and set task_per_process[status.MPI_SOURCE] as recursion times.

For worker:
Just receive the message from farmer and locally computing the AQ by recursion
Then send the result which is composed of the sub-area value and recursion times back to the farmer

*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

#define EPSILON 1e-3
#define F(arg)  cosh(arg)*cosh(arg)*cosh(arg)*cosh(arg)
#define A 0.0
#define B 5.0
#define SLEEPTIME 1

const int LEFT = 0;
const int RIGHT = 1;
const int FARMER = 0;
const int UNFINISH = 0;
const int FINISH = 1;
const int AREA = 0;
const int TIME = 1;

int *tasks_per_process;

//Quad function which returns two double value: an area value and recursion times
double *quad(double left, double right, double fleft, double fright, double lrarea)
{
	double mid, fmid, larea, rarea;
	double *leftReturn, *rightReturn, *temp;

	temp = (double *)malloc(sizeof(double) * 2);
	mid = (left + right) / 2;
	fmid = F(mid);
	larea = (fleft + fmid) * (mid - left) / 2;
	rarea = (fmid + fright) * (right - mid) / 2;
	if (fabs(larea + rarea - lrarea) > EPSILON) 
	{
		leftReturn = quad(left, mid, fleft, fmid, larea);
		rightReturn = quad(mid, right, fmid, fright, rarea);
	}
	else
	{
		temp[0] = larea + rarea;
		temp[1] = 1;
		return temp;
	}
	temp[0] = leftReturn[0] + rightReturn[0];
	temp[1] = leftReturn[1] + rightReturn[1] + 1;
	return temp;
}

double farmer(int numprocs)
{
	//Initialization
	double result = 0.0;
	int i, workers;
	double chunkSize;
	MPI_Status status;
	double left_and_right[2] = { 0.0, 0.0 };
	double area_and_time[2];

	workers = numprocs - 1;
	chunkSize = (B - A) / workers;

	//Set the start and end point of each chunk, then send them to corresponding process
	for (i = 0; i < workers; i++)
	{
		left_and_right[LEFT] = left_and_right[RIGHT];
		left_and_right[RIGHT] = left_and_right[RIGHT] + chunkSize;
		MPI_Send(left_and_right, 2, MPI_DOUBLE, i + 1, i + 1, MPI_COMM_WORLD);
	}

	//Receive message for "workers" times, add the sub-area value to the final result and record the recursion times
	for (i = 0; i < workers; i++)
	{
		MPI_Recv(area_and_time, 2, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		result = result + area_and_time[AREA];
		tasks_per_process[status.MPI_SOURCE] = (int)area_and_time[TIME];
	}

	return result;
}

void worker(int myid)
{
	MPI_Status status;
	double left, right;
	double left_and_right[2];
	double *result;

	//Receive the message from farmer and locally computing the AQ by recursion
	//Then send the result which is composed of the sub-area value and recursion times back to the farmer
	MPI_Recv(left_and_right, 2, MPI_DOUBLE, FARMER, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	left = left_and_right[LEFT];
	right = left_and_right[RIGHT];
	result = quad(left, right, F(left), F(right), (F(left) + F(right)) * (right - left) / 2);
	MPI_Send(result, 2, MPI_DOUBLE, FARMER, FINISH, MPI_COMM_WORLD);

}

int main(int argc, char **argv) {
	int i, myid, numprocs;
	double area, a, b;

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD, &myid);

	if (numprocs < 2) {
		fprintf(stderr, "ERROR: Must have at least 2 processes to run\n");
		MPI_Finalize();
		exit(1);
	}

	if (myid == 0) { // Farmer
		// init counters
		tasks_per_process = (int *)malloc(sizeof(int)*(numprocs));
		for (i = 0; i < numprocs; i++) {
			tasks_per_process[i] = 0;
		}
	}

	if (myid == 0) { // Farmer
		area = farmer(numprocs);
	}
	else { //Workers
		worker(myid);
	}

	if (myid == 0) {
		fprintf(stdout, "Area=%lf\n", area);
		fprintf(stdout, "\nTasks Per Process\n");
		for (i = 0; i < numprocs; i++) {
			fprintf(stdout, "%d\t", i);
		}
		fprintf(stdout, "\n");
		for (i = 0; i < numprocs; i++) {
			fprintf(stdout, "%d\t", tasks_per_process[i]);
		}
		fprintf(stdout, "\n");
		free(tasks_per_process);
	}
	MPI_Finalize();
	return 0;
}
