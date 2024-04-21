#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

typedef struct {
    void *data;
    _Atomic int ref_count;
} rcu_node_t;

void rcu_node_init(rcu_node_t *node, void *data)
{
    node->data = data;
    node->ref_count = 1;
}

void rcu_node_inc_ref_cnt(rcu_node_t *node)
{
    atomic_fetch_add_explicit(&(node->ref_count), 1, memory_order_relaxed);
}

void free_rcu_node(rcu_node_t *node)
{
    if (atomic_fetch_sub_explicit(&(node->ref_count), 1, memory_order_release) == 1)
    {
        // ensure that acquire is used here to synchronize with all previous decrements of the reference count
        atomic_thread_fence(memory_order_acquire);
        if (node->data)
            free(node->data);
        node->data = NULL;
        free(node);
    }
}

typedef struct rcu_stack_node_t {
    rcu_node_t *node_ptr;
    struct rcu_stack_node_t *next;
} rcu_stack_node_t;

void free_rcu_stack_node(rcu_stack_node_t *sn)
{
    while (sn != NULL)
    {
        struct rcu_stack_node_t *next_sn = sn->next;
        if (sn->node_ptr != NULL)
        {
            free_rcu_node(sn->node_ptr);
            sn->node_ptr = NULL;
            sn->next = NULL;
        }
        free(sn);
        sn = next_sn;
    }
}

typedef struct {
    rcu_node_t *_Atomic data_ptr;
    _Atomic int state;
    _Atomic bool epoch_flag;
    rcu_stack_node_t *_Atomic cur_epoch_head;
    rcu_stack_node_t *_Atomic final_epoch_head;
} rcu_t;

void rcu_init(rcu_t *rcu, rcu_node_t *init_data)
{
    atomic_store(&(rcu->data_ptr), init_data);
    rcu->state = 0;
    rcu->epoch_flag = false;

    rcu_stack_node_t *cur_epoch_head = (rcu_stack_node_t*)malloc(sizeof(rcu_stack_node_t));
    cur_epoch_head->node_ptr = NULL;
    cur_epoch_head->next = NULL;

    rcu_stack_node_t *final_epoch_head = (rcu_stack_node_t*)malloc(sizeof(rcu_stack_node_t));
    final_epoch_head->node_ptr = NULL;
    final_epoch_head->next = NULL;

    atomic_store(&(rcu->cur_epoch_head), cur_epoch_head);
    atomic_store(&(rcu->final_epoch_head), final_epoch_head);
}

rcu_node_t *rcu_read(rcu_t *rcu)
{
    // increase the state to signal we are entering critical section
    atomic_fetch_add_explicit(&(rcu->state), 1, memory_order_relaxed);

    // read whatever is currently stored in the rcu data structure
    rcu_node_t *res = atomic_load_explicit(&(rcu->data_ptr), memory_order_relaxed);
    rcu_node_inc_ref_cnt(res);

    if (
        atomic_fetch_sub_explicit(&(rcu->state), 1, memory_order_release) == 1 && 
        !atomic_exchange_explicit(&(rcu->epoch_flag), true, memory_order_release)
    ) {
        // ensure we synchronize with all other threads that have updated the value of 'state' and 'epoch_flag'
        atomic_thread_fence(memory_order_acquire);

        // new sentinel node
        rcu_stack_node_t *new_cur_epoch_head = (rcu_stack_node_t*)malloc(sizeof(rcu_stack_node_t));
        new_cur_epoch_head->node_ptr = NULL;
        new_cur_epoch_head->next = NULL;

        if (!new_cur_epoch_head)
        {
            fprintf(stderr, "%s:%s%d error allocating new epoch stack\n", __FILE__, __FUNCTION__, __LINE__);
            exit(1);
        }

        rcu_stack_node_t *old_cur_epoch_head = atomic_exchange_explicit(&(rcu->cur_epoch_head), new_cur_epoch_head, memory_order_relaxed);
        rcu_stack_node_t *old_final_epoch_head = atomic_exchange_explicit(&(rcu->final_epoch_head), old_cur_epoch_head, memory_order_relaxed);
        
        // deallocate old_final_epoch_head
        free_rcu_stack_node(old_final_epoch_head);

        // finally signal that epoch has finished
        atomic_exchange_explicit(&(rcu->epoch_flag), false, memory_order_release);
    }

    return res;
}

void rcu_push(rcu_t *rcu, rcu_node_t *node)
{
    rcu_stack_node_t *neo = (rcu_stack_node_t*)malloc(sizeof(rcu_stack_node_t));
    neo->node_ptr = node;
    rcu_stack_node_t *cur_head = atomic_load_explicit(&(rcu->cur_epoch_head), memory_order_relaxed);
    neo->next = cur_head;

    while (!atomic_compare_exchange_strong_explicit(&(rcu->cur_epoch_head), &cur_head, neo, memory_order_relaxed, memory_order_relaxed))
        neo->next = cur_head;
}

void rcu_update(rcu_t *rcu, rcu_node_t *neo)
{
    rcu_node_t *cur_data = atomic_load_explicit(&(rcu->data_ptr), memory_order_relaxed);
    while (!atomic_compare_exchange_strong_explicit(&(rcu->data_ptr), &cur_data, neo, memory_order_relaxed, memory_order_relaxed));
    rcu_push(rcu, cur_data);
}


int main(int argc, char **argv)
{
    int test_data[26] = {0};
    

    return 0;
}