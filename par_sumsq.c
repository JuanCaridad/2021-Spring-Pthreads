/*
 * par_sumsq.c
 *
 * CS 446 Project 5 (Pthreads)
 *
 * Compile with --std=c99
 *
 * Juan Caridad
 */

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

//aggreagate variables
long sum = 0;
long odd = 0;
long min = INT_MAX;
long max = INT_MIN;

//Nodes in task queue, and holds val with pointer to next node
typedef struct Task_Queue_Node{
	long val;
	struct Task_Queue_Node* next;
} Task_Queue_Node;

//Task Queue(Linked List)
typedef struct Task_Queue{
	Task_Queue_Node* head;
} Task_Queue;

//function prototypes
void calculate_square(long number);

//worker function
void* routine(void* param);

//queue functions
Task_Queue* createQueue();
void deleteQueue(Task_Queue* queue);
bool pushTask(Task_Queue* queue, long task_val);
long popTask(Task_Queue* queue);

//mutex for aggregate stats and for task queue
pthread_mutex_t lockAggregate = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lockQueue = PTHREAD_MUTEX_INITIALIZER;

//var to know if tasks are read, which master thread uses to update workers
//volatile since no licks used for var
volatile bool done = false;

//conditional var singled by master when items added to queue
pthread_cond_t cond_new_task = PTHREAD_COND_INITIALIZER;
/*
 * update global aggreagate variables given number
 */
void calculate_square(long number)
{
	//calculate the square
	const long the_square = number * number;

	//ok that was not so hard, but let's pretend it was
	//simylate how hard it is to square this number!
	sleep(number);

	pthread_mutex_lock(&lockAggregate);
	//let's add this to our (global) sum
	sum += the_square;

	//now we also tabulate some (meaningless) statistics
	if(number % 2 == 1){
	    //how many of our numbers were odd?
	    odd++;
	}

	// what was the smalles one we had to deal with?
	if(number < min){
	    min = number;
	}

	// and what was the biggest one?
	if(number > max){
	    max = number;
	}
	pthread_mutex_unlock(&lockAggregate);
}


int main(int argc, char* argv[])
{
	// check and parse command line options
	if(argc != 3){
	   printf("Usage: par_sumsq <infile> <num_workers>\n");
	   exit(EXIT_FAILURE);
	}
	const char* fn = argv[1];
	unsigned long num_workers = strtol(argv[2], NULL, 10);

	//load numbers and add them to the queue
	FILE* fin = fopen(fn, "r");

	char action;
	long num;

	volatile Task_Queue* task_queue = createQueue();
	bool error = false;

	pthread_t workers[num_workers];
	for(unsigned long i = 0; i < num_workers; ++i){
		pthread_create(&workers[i], NULL, routine, (void*)task_queue);
	}

	while(fscanf(fin, "%c %ld\n", &action, &num) == 2){
		if(action == 'p'){	//process, do some work
			pthread_mutex_lock(&lockQueue);
			if(!pushTask((Task_Queue*)task_queue, num)){
				error = true;
			}
			pthread_cond_signal(&cond_new_task);
			pthread_mutex_unlock(&lockQueue);
		}else if(action == 'w'){	//wait, nothing new happening
			sleep(num);
		}
	}

	fclose(fin);
	while(task_queue->head){
		//busy wait
	}

	done = true;
	pthread_mutex_lock(&lockQueue);
	pthread_cond_broadcast(&cond_new_task);
	pthread_mutex_unlock(&lockQueue);

	void* routineVal;
	for(unsigned long i = 0; i < num_workers; ++i){
		error |= (pthread_join(workers[i], &routineVal) != 0);
		error |= ((long)routineVal != EXIT_SUCCESS);
	}

	deleteQueue((Task_Queue*)task_queue);

	//print results
	printf("%ld %ld %ld %ld\n", sum, odd, min, max);

	//clean up and return
	return EXIT_SUCCESS;

}


void* routine(void* param)
{
	Task_Queue* taskQueue = (Task_Queue*)param;

	while(!done)
	{
		//Grab queuue lock
		pthread_mutex_lock(&lockQueue);
		while(!taskQueue->head && !done)
			pthread_cond_wait(&cond_new_task, &lockQueue);

		if(done)
		{
			pthread_mutex_unlock(&lockQueue);
			break;
		}

		const long num = popTask(taskQueue);
		pthread_mutex_unlock(&lockQueue);

		calculate_square(num);
	}

	return EXIT_SUCCESS;
}

Task_Queue* createQueue()
{
	Task_Queue* q = (Task_Queue*)malloc(sizeof(struct Task_Queue));
	if(!q)
		return q;

	q->head = NULL;

	return q;
}

void deleteQueue(Task_Queue* queue)
{
	if(!queue)
		return;

	while(queue->head)
		popTask(queue);

	free(queue);
}

bool pushTask(Task_Queue* queue, long task_val)
{
	//Create new node
	struct Task_Queue_Node* node = (struct Task_Queue_Node*)malloc(sizeof(struct Task_Queue_Node));
	if(!node)
		return false;
	node->val = task_val;
	node->next = NULL;

	if(!queue->head)
	{
		//Node is first
		queue->head = node;
	}
	else
	{
		//Add node at back of queue
		struct Task_Queue_Node* tail = queue->head;
		while(tail->next){
			tail = tail->next;
		}
		tail->next = node;
	}
	return true;
}

long popTask(Task_Queue* queue)
{
	//Remove head from queue
	struct Task_Queue_Node* node = queue->head;
	if(node)
	{
		queue->head = node->next;
	}

	if(!node)
	{
		return 0;
	}

	const long num = node->val;
	free(node);
	return num;
}
