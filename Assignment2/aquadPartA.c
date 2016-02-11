/*
Result:

Area=7583461.801486

Tasks Per Process
0       1       2       3       4
0       1627    1687    1623    1630

First of all, for compiling convinence, I copy the code of stack.h and stack.c into this file
Therefore, the compile command is:
/usr/lib64/openmpi/bin/mpicc -o aquadPartA aquadPartA.c

I have defined some const, such as LEFT,FINISH, in order to make the code more clear.

For farmer:
I use stack to store the task.
An idle array used to show whether the process is working or idle.
The integer variable idles shows how many processes are idle.
Before the loop, push the first task into bag and send it to process 1.
The while loop loops when there are more tasks or any process is still working.
At the beginning of the while loop, the farmer receives message from any source(MPI_ANY_SOURCE) with any tag(MPI_ANY_TAG).
Once the message received, the source process should be idle.
Therefore, increase idles by 1 and set the idle[source] as 1.
If the tag of message is FINISH, it means this message contains a sub-area value, add the value to the final result.
Otherwise, the process must have sent 2 task descriptions back to the farmer, push the current task into bag, receive another task from same source and push again.
After that, if there are any task in bag and any idle process, send the task to the process with UNFINISH tag.
Decrease the idles by 1, set the idle[(idle process)] as 0, increase task_per_process[(idle process)] by 1.
When the while loop stops, send a "stop" message to all processes with tag FINISH.

For worker:
The worker is simpler than farmer, which is an infinite loop.
In the loop, worker receives the message from farmer(use FARMER as its source) with any tag(MPI_ANY_TAG).
If the tag is UNFINISH, computing the AQ, send back a sub-area value or two task descriptions.
Or the tag is FINISH, just stop the loop.

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

//Const values used to make code more clear
const int LEFT = 0;
const int RIGHT = 1;
const int UNFINISH = 0;
const int FINISH = 1;
const int FARMER = 0;
int *tasks_per_process;

typedef struct stack_node_tag stack_node;
typedef struct stack_tag stack;

struct stack_node_tag {
	double     data[2];
	stack_node *next;
};

struct stack_tag {
	stack_node     *top;
};

// creating a new stack
stack * new_stack()
{
	stack *n;

	n = (stack *)malloc(sizeof(stack));

	n->top = NULL;

	return n;
}

// cleaning up after use
void free_stack(stack *s)
{
	free(s);
}

// Push data to stack, data has to be an array of 2 doubles
void push(double *data, stack *s)
{
	stack_node *n;
	n = (stack_node *)malloc(sizeof(stack_node));
	n->data[0] = data[0];
	n->data[1] = data[1];

	if (s->top == NULL) {
		n->next = NULL;
		s->top = n;
	}
	else {
		n->next = s->top;
		s->top = n;
	}
}

// Pop data from stack
double * pop(stack * s)
{
	stack_node * n;
	double *data;

	if (s == NULL || s->top == NULL) {
		return NULL;
	}
	n = s->top;
	s->top = s->top->next;
	data = (double *)malloc(2 * (sizeof(double)));
	data[0] = n->data[0];
	data[1] = n->data[1];
	free(n);

	return data;
}

// Check for an empty stack
int is_empty(stack * s) {
	return (s == NULL || s->top == NULL);
}

double farmer(int numprocs) {
	// You must complete this function

	//Initialization
	stack * bag_of_task = new_stack();
	int i, tag, source, workers;
	double result = 0.0;
	MPI_Status status;
	double left_and_right[2] = { A, B };

	//Push the first task into bag
	push(left_and_right, bag_of_task);
	workers = numprocs - 1;
	int * idle = (int *)malloc(sizeof(int) * workers);
	//The first process is working and the other 3 processes are idle
	int idles = 3;
	for (i = 1; i < workers; i++)
	{
		idle[i] = 1;
	}
	idle[0] = 0;
	MPI_Send(pop(bag_of_task), 2, MPI_DOUBLE, 1, UNFINISH, MPI_COMM_WORLD);
	tasks_per_process[1]++;

	//When there are more tasks or any process is still working, loop
	while (!is_empty(bag_of_task) || idles < workers)
	{
		//Receive message and set the message source process idle
		MPI_Recv(left_and_right, 2, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		source = status.MPI_SOURCE;
		tag = status.MPI_TAG;
		idles++;
		idle[source - 1] = 1;
		//If the tag of message is FINISH, add the return value to the final AQ result
		if (tag == FINISH)
		{
			result = result + left_and_right[LEFT];
		}
		//Otherwise, the tag of message is UNFINISH, which means there should be another task description
		//Push the current one and receive another, then push again
		else
		{
			push(left_and_right, bag_of_task);
			MPI_Recv(left_and_right, 2, MPI_DOUBLE, source, tag, MPI_COMM_WORLD, &status);
			push(left_and_right, bag_of_task);
		}
		i = 0;
		//When there are any idle process and any task in bag, send it to the worker
		while (! is_empty(bag_of_task) && idles > 0)
		{
			if (idle[i])
			{
				idles--;
				idle[i] = 0;
				tasks_per_process[i + 1]++;
				MPI_Send(pop(bag_of_task), 2, MPI_DOUBLE, i + 1, UNFINISH, MPI_COMM_WORLD);
			}
			i = (i + 1) % workers;
		}
	}
	free(idle);
	left_and_right[LEFT] = 0.0;
	left_and_right[RIGHT] = 0.0;
	//After all tasks done, send messages with FINISH tag to all workers to stop them
	for (i = 0; i < workers; i++)
	{
		MPI_Send(left_and_right, 2, MPI_DOUBLE, i + 1, FINISH, MPI_COMM_WORLD);
	}

	return result;
}

void worker(int myid) {
	// You must complete this function

	MPI_Status status;
	double left_and_right[2] = { 0.0, 0.0 };

	for (;;)
	{
		//Receive task from farmer
		MPI_Recv(left_and_right, 2, MPI_DOUBLE, FARMER, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if (status.MPI_TAG != FINISH)
		{
			usleep(SLEEPTIME);
			double left, right, lrarea, mid, larea, rarea;
			left = left_and_right[LEFT];
			right = left_and_right[RIGHT];
			lrarea = (F(left) + F(right))*(right - left) / 2;
			mid = (left + right) / 2;
			larea = (F(left) + F(mid)) * (mid - left) / 2;
			rarea = (F(mid) + F(right))*(right - mid) / 2;
			//Send two task descriptions back to farmer with UNFINISH tag
			if (fabs(larea + rarea - lrarea) > EPSILON)
			{
				left_and_right[LEFT] = left;
				left_and_right[RIGHT] = mid;
				MPI_Send(left_and_right, 2, MPI_DOUBLE, FARMER, UNFINISH, MPI_COMM_WORLD);
				left_and_right[LEFT] = mid;
				left_and_right[RIGHT] = right;
				MPI_Send(left_and_right, 2, MPI_DOUBLE, FARMER, UNFINISH, MPI_COMM_WORLD);
			}
			//Send one sub-area value back to farmer with FINISH tag
			else
			{
				left_and_right[LEFT] = larea + rarea;
				left_and_right[RIGHT] = 0;
				MPI_Send(left_and_right, 2, MPI_DOUBLE, FARMER, FINISH, MPI_COMM_WORLD);
			}
		}
		else
		{
			break;
		}
	}
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
