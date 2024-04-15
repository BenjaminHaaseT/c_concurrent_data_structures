#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

typedef struct {
    void *data;
    _Atomic int ref_count;
} rcu_node_t;

void rcu_node_t_init(rcu_node_t *node, void *data)
{
    node->data = data;
    node->ref_count = 1;
}

void rcu_node_t_inc_ref_cnt(rcu_node_t *node)
{
    atomic_fetch_add_explicit(&(node->ref_count), 1, memory_order_relaxed);
}

void rcu_node_t_release(rcu_node_t *node)
{
    if (atomic_fetch_sub_explicit(&(node->ref_count), 1, memory_order_release) == 1)
    {
        // ensure that acquire is used here to synchronize with all previous decrements of the reference count
        atomic_thread_fence(memory_order_acquire);
        free(node->data);
        node->data = NULL;
    }
}


typedef struct {
    rcu_node_t _Atomic *data_ptr;
    _Atomic int state;
    _Atomic bool epoch_flag;
} rcu_t;

int main(int argc, char **argv)
{

    return 0;
}