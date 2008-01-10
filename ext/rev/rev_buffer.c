#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <time.h>

/* Number of bytes in each node's buffer */
#define NODE_SIZE 65536

/* Maximum age of a buffer node in a memory pool, in seconds */
#define MAX_AGE 900

struct buffer_node {
  unsigned start, end;
  unsigned char data[NODE_SIZE];
  struct buffer_node *next;
  time_t last_used_at;
};

struct buffer {
  unsigned size;
  struct buffer_node *head, *tail;
  struct buffer_node *pool_head, *pool_tail;
};

void *xmalloc(int len)
{
  return malloc(len);
}

struct buffer *buffer_new(void)
{
  struct buffer *buf;
  buf = (struct buffer *)xmalloc(sizeof(struct buffer));
  buf->head = buf->tail = buf->pool_head = buf->pool_tail = 0;
  buf->size = 0;

  return buf;
}

void buffer_clear(struct buffer *buf)
{
  struct buffer_node *tmp;

  while(buf->head) {
    tmp = buf->head;
    buf->head = tmp->next;
    free(tmp);
  }

  while(buf->pool_head) {
    tmp = buf->pool_head;
    buf->pool_head = tmp->next;
    free(tmp);
  }
}

void buffer_free(struct buffer *buf) 
{
  buffer_clear(buf);
  free(buf);
}

/* Run through the pool and find elements that haven't been used for awhile
 * This is the only case which is O(n) for an n sized pool.  It could be
 * O(1) if the whole implementation were switched to doubly linked lists.  
 * But uhh, honestly, who cares?  It's a stupid garbage collector */
void buffer_gc(struct buffer *buf)
{
  struct buffer_node *cur, *tmp;
  time_t now;
  time(&now);

  while(buf->pool_head && now - buf->pool_head->last_used_at > MAX_AGE) {
    tmp = buf->pool_head;
    buf->pool_head = buf->pool_head->next;
    free(tmp);
  }

  if(!buf->pool_head)
    return;

  cur = buf->pool_head;

  while(cur->next) {
    if(now - cur->next->last_used_at < MAX_AGE)
      continue;

    tmp = cur->next;
    cur->next = tmp->next;
    free(tmp);
  }
}

struct buffer_node *buffer_node_new(struct buffer *buf)
{
  struct buffer_node *node;

  /* Pull from the memory pool if available */
  if(buf->pool_head) {
    node = buf->pool_head;
    buf->pool_head = node->next;
    
    if(node->next)
      node->next = 0;
    else
      buf->pool_tail = 0;
  } else {
    node = (struct buffer_node *)xmalloc(sizeof(struct buffer_node));
    node->next = 0;
  }

  node->start = node->end = 0;
  return node;
}

void buffer_node_free(struct buffer *buf, struct buffer_node *node)
{
  /* Store when the node was freed */
  time(&node->last_used_at);

  node->next = buf->head;
  buf->head = node;
}

void buffer_prepend(struct buffer *buf, char *string, unsigned len)
{
  struct buffer_node *node, *tmp;

  buf->size += len;

  /* If it fits in the beginning of the head */
  if(buf->head && buf->head->start >= len) {
    buf->head->start -= len;
    memcpy(buf->head->data + buf->head->start, string, len);
  } else {
    node = buffer_node_new(buf);
    node->next = buf->head;
    buf->head = node;
    if(!buf->tail) buf->tail = node;

    while(len > NODE_SIZE) {
      memcpy(node->data, string, NODE_SIZE);
      node->end = NODE_SIZE;

      tmp = buffer_node_new(buf);
      tmp->next = node->next;
      node->next = tmp;

      if(buf->tail == node) buf->tail = tmp;
      node = tmp;

      string += NODE_SIZE;
      len -= NODE_SIZE;
    }

    if(len > 0) {
      memcpy(node->data, string, len);
      node->end = len;
    }
  }
}

void buffer_append(struct buffer *buf, char *string, unsigned len)
{
  unsigned nbytes;
  buf->size += len;

  /* If it fits in the remaining space in the tail */
  if(buf->tail && len <= NODE_SIZE - buf->tail->end) {
    memcpy(buf->tail->data + buf->tail->end, string, len);
    buf->tail->end += len;
    return;
  }

  /* Empty list needs initialized */
  if(!buf->head) {
    buf->head = buffer_node_new(buf);
    buf->tail = buf->head;
  }

  /* Build links out of the data */
  while(len > 0) {
    nbytes = NODE_SIZE - buf->tail->end;
    if(len < nbytes) nbytes = len;

    memcpy(buf->tail->data + buf->tail->end, string, nbytes);
    len -= nbytes;
    buf->tail->end += nbytes;

    if(len > 0) {
      buf->tail->next = buffer_node_new(buf);
      buf->tail = buf->tail->next;
    }
  }
}

/*
int main()
{
  struct buffer *buf;
  char *hugeicus = (char *)malloc(80000);

  memset(hugeicus, 'X', 80000);
  buf = buffer_new();

  buffer_prepend(buf, hugeicus, 80000);
  buffer_prepend(buf, "hey ho", 6);
  buffer_append(buf, " it's off to work we go", 23);
  buffer_prepend(buf, "hey ho ", 7);
  buffer_prepend(buf, "foo", 3);
  buffer_append(buf, "bar", 3);
  buffer_append(buf, "baz", 3);
  buffer_prepend(buf, "quux", 4);

  buffer_free(buf);
  free(hugeicus);
  return 0;
}
*/
