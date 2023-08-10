#include "../../include/tool.h"

void destroy_buf(BufType *buf)
{
    if (buf)
    {
        free(buf->start);
        free(buf);
    }
}

unsigned int get_save_frame(Config config)
{
    return (unsigned int)av_q2d(av_inv_q(config.time_base)) * config.save_time;
}

LinkedList *create_linked_list()
{
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    if (list)
        list->head = NULL;
    return list;
}

void free_linked_list(LinkedList *list)
{
    Node *cur = list->head;
    while (cur)
    {
        Node *temp = cur;
        cur = cur->next;
        free(temp->data);
        free(temp);
    }
    free(list);
}

void append_linked_list(LinkedList *list, void *data)
{
    Node *new_node = (Node *)malloc(sizeof(Node));
    if (new_node)
    {
        new_node->data = data;
        new_node->next = NULL;

        if (!list->head)
            list->head = new_node;
        else
        {
            Node *current = list->head;
            while (current->next)
            {
                current = current->next;
            }
            current->next = new_node;
        }
    }
}

void foreach_linked_list(LinkedList *list, void (*func)(void *))
{
    Node *current = list->head;
    while (current)
    {
        func(current->data);
        current = current->next;
    }
}

void *get_linked_list(LinkedList *list, unsigned int index)
{
    unsigned int count = 0;
    Node *current = list->head;
    while (current)
    {
        if (count < index)
        {
            count++;
            current = current->next;
        }
        else if (count == index)
        {
            return current->data;
        }
    }
    LOG(logger, LOG_ERROR, "Index out of range: `%d`", index);
    return NULL;
}

int remove_linked_list(LinkedList *list, unsigned int index)
{
    unsigned int count = 0;
    Node *current = list->head;
    while (current)
    {
        if (count < index - 1)
        {
            count++;
            current = current->next;
        }
        else if (count == index - 1)
        {
            Node *removed_item = current->next;
            current->next = current->next->next;
            removed_item->next = NULL;
            free(removed_item->data);
            free(removed_item);
            return 0;
        }
    }
    return -1;
}