// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include "os_graph.h"
#include "os_threadpool.h"
#include "log/log.h"
#include "utils.h"

#define NUM_THREADS		4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
/* TODO: Define graph synchronization mechanisms. */
pthread_mutex_t sum_mux;
pthread_mutex_t nbr_mux;

/* TODO: Define graph task argument. */
void graph_task(void *node)
{
	os_node_t *curr_node = (os_node_t *) node;

	pthread_mutex_lock(&sum_mux);
	sum += curr_node->info;
	pthread_mutex_unlock(&sum_mux);

	for (unsigned int i = 0; i < curr_node->num_neighbours; i++) {
		pthread_mutex_lock(&nbr_mux);

		if (graph->visited[curr_node->neighbours[i]] == NOT_VISITED) {
			graph->visited[curr_node->neighbours[i]] = PROCESSING;
			pthread_mutex_unlock(&nbr_mux);

			os_task_t *task = create_task(graph_task, (void *) graph->nodes[curr_node->neighbours[i]], free);

			enqueue_task(tp, task);

			graph->visited[curr_node->neighbours[i]] = DONE;
		} else {
			pthread_mutex_unlock(&nbr_mux);
		}
	}
}

static void process_node(unsigned int idx)
{
	os_node_t *node;

	node = graph->nodes[idx];
	sum += node->info;
	graph->visited[idx] = DONE;

	for (unsigned int i = 0; i < node->num_neighbours; i++) {
		pthread_mutex_lock(&nbr_mux);

		if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
			graph->visited[node->neighbours[i]] = PROCESSING;
			pthread_mutex_unlock(&nbr_mux);

			os_task_t *task = create_task(graph_task, (void *) graph->nodes[node->neighbours[i]], free);

			enqueue_task(tp, task);

			graph->visited[node->neighbours[i]] = DONE;
		} else {
			pthread_mutex_unlock(&nbr_mux);
		}
	}
}

int main(int argc, char *argv[])
{
	FILE *input_file;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s input_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	input_file = fopen(argv[1], "r");
	DIE(input_file == NULL, "fopen");

	graph = create_graph_from_file(input_file);

	pthread_mutex_init(&sum_mux, NULL);
	pthread_mutex_init(&nbr_mux, NULL);

	tp = create_threadpool(NUM_THREADS);
	process_node(0);
	wait_for_completion(tp);
	destroy_threadpool(tp);

	printf("%d", sum);

	return 0;
}
