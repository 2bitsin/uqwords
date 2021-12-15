
Short summary
=====

I tried really hard but I couldn't get this to scale much beyond 4 threads, as it exceeds my SATA disk's bandwidth so the problem is I/O bound.
I don't have any way to test it on a faster disk at this moment. The basic idea of how this works is. 

Basically this is implemented in a producer/consumer pattern.

1. I split the file into chunks.
2. Map a few chunks to memory at a time (depending on threads available).
3. Dispatch each chunk to be split into words, into separate threads.
4. Each little task produces a small local set of words
5. Each of the small sets are further merged tournament style (also in a parallel fassion)

I have written a very basic thread pool system with task stealing to handle this.
Yes it's based on the one described by Sean Parent in his talk "Better Code Concurrency"

I could spread the I/O load accross worker threads instead of the producer thread, but that does not give me any more I/O troughput anyway so no real benefit.
