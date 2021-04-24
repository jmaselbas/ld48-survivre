#ifndef RING_BUFFER_H
#define RING_BUFFER_H

struct ring_buffer {
	void *base;
	size_t nmem;
	size_t size;
	volatile size_t head;
	volatile size_t tail;
};

inline static struct ring_buffer
ring_buffer_init(void *base, size_t nmemb, size_t size)
{
	struct ring_buffer rbuf = {
		.base = base,
		.nmem = nmemb,
		.size = size,
	};
	return rbuf;
}

inline static size_t
ring_buffer_fill_count(struct ring_buffer *rbuf)
{
	size_t n = rbuf->nmem;
	size_t h = rbuf->head;
	size_t t = rbuf->tail;

	return (2 * n + h - t) % (2 * n);
}

inline static size_t
ring_buffer_free_count(struct ring_buffer *rbuf)
{
	size_t n = rbuf->nmem;

	return n - ring_buffer_fill_count(rbuf);
}

inline static int
ring_buffer_full(struct ring_buffer *rbuf)
{
	size_t n = rbuf->nmem;
	size_t f = ring_buffer_fill_count(rbuf);

	return f == n;
}

inline static int
ring_buffer_empty(struct ring_buffer *rbuf)
{
	size_t h = rbuf->head;
	size_t t = rbuf->tail;

	return h == t;
}

inline static void *
ring_buffer_read_addr(struct ring_buffer *rbuf)
{
	size_t n = rbuf->nmem;
	size_t t = rbuf->tail;
	size_t i = t % n;

	return rbuf->base + i * rbuf->size;
}

inline static void *
ring_buffer_write_addr(struct ring_buffer *rbuf)
{
	size_t n = rbuf->nmem;
	size_t h = rbuf->head;
	size_t i = h % n;

	return rbuf->base + i * rbuf->size;
}

inline static size_t
ring_buffer_read_size(struct ring_buffer *rbuf)
{
	size_t n = rbuf->nmem;
	size_t t = rbuf->tail;
	size_t a = n - (t % n);
	size_t b = ring_buffer_fill_count(rbuf);

	return MIN(a, b);
}

inline static size_t
ring_buffer_write_size(struct ring_buffer *rbuf)
{
	size_t n = rbuf->nmem;
	size_t h = rbuf->head;
	size_t a = n - (h % n);
	size_t b = ring_buffer_free_count(rbuf);

	return MIN(a, b);
}

inline static void
ring_buffer_read_done(struct ring_buffer *rbuf, size_t nmemb)
{
	size_t n = rbuf->nmem;
	size_t t = rbuf->tail;

	rbuf->tail = (t + nmemb) % (2 * n);
}

inline static void
ring_buffer_write_done(struct ring_buffer *rbuf, size_t nmemb)
{
	size_t n = rbuf->nmem;
	size_t h = rbuf->head;

	rbuf->head = (h + nmemb) % (2 * n);
}

#endif
