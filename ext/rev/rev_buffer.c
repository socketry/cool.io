/*
 * Copyright (C) 2007 Tony Arcieri
 * You may redistribute this under the terms of the Ruby license.
 * See LICENSE for details
 */

#include "ruby.h"
#include "rubyio.h"
#include <assert.h>

#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* Default number of bytes in each node's buffer */
#define DEFAULT_NODE_SIZE 65536
#define MAX_NODE_SIZE			10 * 1024 * 1024

/* Maximum age of a buffer node in a memory pool, in seconds */
#define MAX_AGE 900

struct buffer {
  unsigned size, node_size;
  struct buffer_node *head, *tail;
  struct buffer_node *pool_head, *pool_tail;
};

struct buffer_node {
  unsigned start, end;
  struct buffer_node *next;
  time_t last_used_at;
  unsigned char data[0];
};

/* Module and object handles */
static VALUE mRev = Qnil;
static VALUE cRev_Buffer = Qnil;

/* Data allocators and deallocators */
static VALUE Rev_Buffer_allocate(VALUE klass);
static void Rev_Buffer_mark(struct buffer *);
static void Rev_Buffer_free(struct buffer *);

/* Method implementations */
static VALUE Rev_Buffer_initialize(int argc, VALUE *argv, VALUE self);
static VALUE Rev_Buffer_clear(VALUE self);
static VALUE Rev_Buffer_size(VALUE self);
static VALUE Rev_Buffer_empty(VALUE self);
static VALUE Rev_Buffer_append(VALUE self, VALUE data);
static VALUE Rev_Buffer_prepend(VALUE self, VALUE data);
static VALUE Rev_Buffer_read(int argc, VALUE *argv, VALUE self);
static VALUE Rev_Buffer_write_to(VALUE self, VALUE io);

/* Prototypes for internal functions */
static struct buffer *buffer_new(void);
static void buffer_clear(struct buffer *buf);
static void buffer_free(struct buffer *buf);
static void buffer_gc(struct buffer *buf);
static void buffer_prepend(struct buffer *buf, char *str, unsigned len);
static void buffer_append(struct buffer *buf, char *str, unsigned len);
static void buffer_read(struct buffer *buf, char *str, unsigned len);
static int buffer_write_to(struct buffer *buf, int fd);

void Init_rev_buffer()
{
  mRev = rb_define_module("Rev");
  cRev_Buffer = rb_define_class_under(mRev, "Buffer", rb_cObject);
  rb_define_alloc_func(cRev_Buffer, Rev_Buffer_allocate);

  rb_define_method(cRev_Buffer, "initialize", Rev_Buffer_initialize, -1);
  rb_define_method(cRev_Buffer, "clear", Rev_Buffer_clear, 0);
	rb_define_method(cRev_Buffer, "size", Rev_Buffer_size, 0);
	rb_define_method(cRev_Buffer, "empty?", Rev_Buffer_empty, 0);
  rb_define_method(cRev_Buffer, "append", Rev_Buffer_append, 1);
  rb_define_method(cRev_Buffer, "prepend", Rev_Buffer_prepend, 1);
  rb_define_method(cRev_Buffer, "read", Rev_Buffer_read, -1);
	rb_define_method(cRev_Buffer, "write_to", Rev_Buffer_write_to, 1);
}

static VALUE Rev_Buffer_allocate(VALUE klass)
{
  return Data_Wrap_Struct(klass, Rev_Buffer_mark, Rev_Buffer_free, buffer_new());
}

static void Rev_Buffer_mark(struct buffer *buf)
{
	/* Walks the pool of unused chunks and frees any that are beyond a certain age */
  buffer_gc(buf);
}

static void Rev_Buffer_free(struct buffer *buf)
{
  buffer_free(buf);
}

/**
 *  call-seq:
 *    Rev::Buffer.new(size = DEFAULT_NODE_SIZE) -> Rev::Buffer
 * 
 * Create a new Rev::Buffer with linked segments of the given size
 */
static VALUE Rev_Buffer_initialize(int argc, VALUE *argv, VALUE self)
{
	VALUE node_size_obj;
	int node_size;
	struct buffer *buf;
	
	if(rb_scan_args(argc, argv, "01", &node_size_obj) == 1) {
		node_size = NUM2INT(node_size_obj);
		
		if(node_size < 1 || node_size > MAX_NODE_SIZE)
			rb_raise(rb_eArgError, "invalid buffer size");
		
		Data_Get_Struct(self, struct buffer, buf);
		
		/* Make sure we're not changing the buffer size after data has been allocated */
		assert(!buf->head);
		assert(!buf->pool_head);
		
		buf->node_size = node_size;
	}
	
	return Qnil;
}


/**
 *  call-seq:
 *    Rev::Buffer.clear -> nil
 * 
 * Clear all data from the Rev::Buffer
 */
static VALUE Rev_Buffer_clear(VALUE self)
{
	struct buffer *buf;
	Data_Get_Struct(self, struct buffer, buf);
	
	buffer_clear(buf);
	
	return Qnil;
}

static VALUE Rev_Buffer_size(VALUE self) 
{
	struct buffer *buf;
	Data_Get_Struct(self, struct buffer, buf);
	
	return INT2NUM(buf->size);
}

static VALUE Rev_Buffer_empty(VALUE self) 
{
	struct buffer *buf;
	Data_Get_Struct(self, struct buffer, buf);
	
	return buf->size > 0 ? Qfalse : Qtrue;	
}

static VALUE Rev_Buffer_append(VALUE self, VALUE data)
{
	struct buffer *buf;
	Data_Get_Struct(self, struct buffer, buf);
	
	/* Is this needed?  Never seen anyone else do it... */
	data = rb_convert_type(data, T_STRING, "String", "to_str");
	buffer_append(buf, RSTRING_PTR(data), RSTRING_LEN(data));
	
	return data;
}

static VALUE Rev_Buffer_prepend(VALUE self, VALUE data)
{
	struct buffer *buf;
	Data_Get_Struct(self, struct buffer, buf);
	
	data = rb_convert_type(data, T_STRING, "String", "to_str");
	buffer_prepend(buf, RSTRING_PTR(data), RSTRING_LEN(data));
	
	return data;
}

static VALUE Rev_Buffer_read(int argc, VALUE *argv, VALUE self)
{
	VALUE length_obj, str;
	long length;
	struct buffer *buf;
	
	Data_Get_Struct(self, struct buffer, buf);
	
	if(rb_scan_args(argc, argv, "01", &length_obj) == 1) {
		length = NUM2INT(length_obj);
	} else {
		if(buf->size == 0)
			return rb_str_new2("");
			
		length = buf->size;
	}
	
	if(length > buf->size)
		length = buf->size;
		
	if(length < 1)
		rb_raise(rb_eArgError, "length must be greater than zero");
		
	str = rb_str_buf_new(length);
	
	/* FIXME There really has to be a better way to do this */
	buffer_read(buf, RSTRING_PTR(str), length);
	RSTRING(str)->as.heap.len = length; /* <-- something tells me this is bad */
	RSTRING_PTR(str)[length] = '\0'; /* sentinel */
	
	return str;
}

static VALUE Rev_Buffer_write_to(VALUE self, VALUE io) {
	struct buffer *buf;
	rb_io_t *fptr;
	
	Data_Get_Struct(self, struct buffer, buf);
	GetOpenFile(rb_convert_type(io, T_FILE, "IO", "to_io"), fptr);
	
	return INT2NUM(buffer_write_to(buf, fptr->fd));
}

/*
 * Ruby bindings end here.  Below is the actual implementation of 
 * the underlying data structures.
 */

/* Yay fun debugging crap */
void buffer_debug(struct buffer *buf)
{
	struct buffer_node *node;
	char printbuf[DEFAULT_NODE_SIZE + 1];
	
	printf("\n-- head: %p tail: %p\n", buf->head, buf->tail);
	
	node = buf->head;
	while(node) {
		printf(" - node: start: %d end: %d\n", node->start, node->end);
		
		assert(node->end - node->start < DEFAULT_NODE_SIZE);
		memset(printbuf, 0, DEFAULT_NODE_SIZE + 1);
		memcpy(printbuf, node->data, node->end - node->start);
	
		printf("   data: %s\n", printbuf);
		printf("   next: %p\n", node->next);
		
		node = node->next;
	}
}

static struct buffer *buffer_new(void)
{
  struct buffer *buf;
  buf = (struct buffer *)xmalloc(sizeof(struct buffer));
  buf->head = buf->tail = buf->pool_head = buf->pool_tail = 0;
  buf->size = 0;
  buf->node_size = DEFAULT_NODE_SIZE;

  return buf;
}

static void buffer_clear(struct buffer *buf)
{
  struct buffer_node *tmp;

  while(buf->head) {
    tmp = buf->head;
    buf->head = tmp->next;
    free(tmp);
  }

	buf->tail = 0;
	buf->size = 0;
}

static void buffer_free(struct buffer *buf) 
{
	struct buffer_node *tmp;
	
  buffer_clear(buf);

  while(buf->pool_head) {
    tmp = buf->pool_head;
    buf->pool_head = tmp->next;
    free(tmp);
  }

  free(buf);
}

/* Run through the pool and find elements that haven't been used for awhile
 * This is the only case which is O(n) for an n sized pool.  It could be
 * O(1) if the whole implementation were switched to doubly linked lists.  
 * But uhh, honestly, who cares?  It's a stupid garbage collector */
static void buffer_gc(struct buffer *buf)
{
  struct buffer_node *cur, *tmp;
  time_t now;
  time(&now);

  while(buf->pool_head && now - buf->pool_head->last_used_at >= MAX_AGE) {
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

static struct buffer_node *buffer_node_new(struct buffer *buf)
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
    node = (struct buffer_node *)xmalloc(sizeof(struct buffer_node) + buf->node_size);
    node->next = 0;
  }

  node->start = node->end = 0;
  return node;
}

static void buffer_node_free(struct buffer *buf, struct buffer_node *node)
{
  /* Store when the node was freed */
  time(&node->last_used_at);

  node->next = buf->pool_head;
  buf->pool_head = node;

	if(!buf->pool_tail)
		buf->pool_tail = node;
}

static void buffer_prepend(struct buffer *buf, char *str, unsigned len)
{
  struct buffer_node *node, *tmp;
  buf->size += len;

  /* If it fits in the beginning of the head */
  if(buf->head && buf->head->start >= len) {
    buf->head->start -= len;
    memcpy(buf->head->data + buf->head->start, str, len);
  } else {
    node = buffer_node_new(buf);
    node->next = buf->head;
    buf->head = node;
    if(!buf->tail) buf->tail = node;

    while(len > buf->node_size) {
      memcpy(node->data, str, buf->node_size);
      node->end = buf->node_size;

      tmp = buffer_node_new(buf);
      tmp->next = node->next;
      node->next = tmp;

      if(buf->tail == node) buf->tail = tmp;
      node = tmp;

      str += buf->node_size;
      len -= buf->node_size;
    }

    if(len > 0) {
      memcpy(node->data, str, len);
      node->end = len;
    }
  }
}

static void buffer_append(struct buffer *buf, char *str, unsigned len)
{
  unsigned nbytes;
  buf->size += len;

  /* If it fits in the remaining space in the tail */
  if(buf->tail && len <= buf->node_size - buf->tail->end) {
    memcpy(buf->tail->data + buf->tail->end, str, len);
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
    nbytes = buf->node_size - buf->tail->end;
    if(len < nbytes) nbytes = len;

    memcpy(buf->tail->data + buf->tail->end, str, nbytes);
    len -= nbytes;
    buf->tail->end += nbytes;

    if(len > 0) {
      buf->tail->next = buffer_node_new(buf);
      buf->tail = buf->tail->next;
    }
  }
}

static void buffer_read(struct buffer *buf, char *str, unsigned len)
{
  unsigned nbytes;
  struct buffer_node *tmp;
	
  while(buf->size > 0 && len > 0) {
    nbytes = buf->head->end - buf->head->start;
    if(len < nbytes) nbytes = len;

    memcpy(str, buf->head->data + buf->head->start, nbytes);
		str += nbytes;
    len -= nbytes;

    buf->head->start += nbytes;
    buf->size -= nbytes;
		
    if(buf->head->start == buf->head->end) {
      tmp = buf->head;
      buf->head = tmp->next;
      buffer_node_free(buf, tmp);

			if(!buf->head) buf->tail = 0;
    }
  }
}

static int buffer_write_to(struct buffer *buf, int fd)
{
  int written, total_written = 0;
  struct buffer_node *tmp;

  while(buf->head) {
    written = write(fd, buf->head->data + buf->head->start, buf->head->end - buf->head->start);

    /* If the write failed... */
    if(written < -1) {
      if(errno == EAGAIN)
        errno = 0;

      return total_written;
    }

    total_written += written;
    buf->size -= written;

    /* If the write blocked... */
    if(written < buf->head->end - buf->head->start) {
      buf->head->start += written;
      return total_written;
    }

    /* Otherwise we wrote the whole buffer */
    tmp = buf->head;
    buf->head = tmp->next;
    buffer_node_free(buf, tmp);

		if(!buf->head) buf->tail = 0;
  }

  return total_written;
}