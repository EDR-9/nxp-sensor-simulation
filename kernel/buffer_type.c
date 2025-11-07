/*********************************************************************************************************************************
Ring buffer implementation for sensor samples

- Initialization
- Check if buffer is empty or full
- Enqueue/Push
- Dequeue/Pop
- Print buffer values

*********************************************************************************************************************************/


#include "buffer_type.h"



void rbuffer_init( struct TempBuffer_t* buffer )
{
    /* Both buffer indexes start from zero */
    buffer->_head = 0;
    buffer->_tail = 0;
    memset(buffer->_samples, 0, sizeof(buffer->_samples));
    pr_info("buffer init\n");
}


__u8 rbuffer_empty( struct TempBuffer_t* buffer )
{   
    if ( buffer->_head == buffer->_tail )
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


__u8 rbuffer_full( struct TempBuffer_t* buffer )
{
    /* When buffer is full, head goes back to zero (after making a turn), it's needed to consider
    the buffer size and how head changes, so next_head = (current_head + 1) % BUFFER_SIZE */

    if ( ((buffer->_head + 1) % TEMP_BUFFER_SIZE) == buffer->_tail )
    {
        return 1;
    }
    else
    {
        return 0;
    }
}


__u8 rbuffer_enqueue( struct TempBuffer_t* buffer, struct simtemp_sample* newTempSample )
{
    if ( rbuffer_full(buffer) )
    {
        pr_info_ratelimited("%s: buffer has not been accessed, no element has been read ('dequeued'). head=%d tail=%d\n", DRIVER_KERNEL_ID, buffer->_head, buffer->_tail);
        return 1;
    }

    //pr_info("%s: current head = %d\tsample received = %d\n", DRIVER_KERNEL_ID, buffer->_head, newTempSample->temp_mC);
    buffer->_samples[buffer->_head] = *newTempSample;
    buffer->_head = (buffer->_head + 1) % TEMP_BUFFER_SIZE;
    //pr_info("%s: next head (available position): %d\n", DRIVER_KERNEL_ID, buffer->_head);

    return 0;
}


__u8 rbuffer_dequeue( struct TempBuffer_t* buffer, struct simtemp_sample* extUserRead )
{
    if ( rbuffer_empty(buffer) )
    {
        pr_info_ratelimited("%s: buffer is empty, there's no element to be read. head=%d tail=%d\n", DRIVER_KERNEL_ID, buffer->_head, buffer->_tail);
        return 1;
    }

    //pr_info("%s: current tail = %d\tsample read = %d\n", DRIVER_KERNEL_ID, buffer->_tail, buffer->_samples[buffer->_tail].temp_mC);
    *extUserRead = buffer->_samples[buffer->_tail];
    buffer->_tail = (buffer->_tail + 1) % TEMP_BUFFER_SIZE;
    //pr_info("%s: next tail: %d\n", DRIVER_KERNEL_ID, buffer->_tail);

    return 0;
}


void rbuffer_show( struct TempBuffer_t* buffer, __u8 bufferTraversalType )
{
    pr_info("%s: buffer data:\n", DRIVER_KERNEL_ID);
    __u8 bufferIndex;

    if ( bufferTraversalType == TEMP_BUFFER_LOGICAL_TRAVERSAL )
    {
        bufferIndex = buffer->_tail;
    }
    else
    {
        bufferIndex = 0;
    }

    while ( bufferIndex != buffer->_head )
    {
        struct simtemp_sample tmp = buffer->_samples[bufferIndex];
        pr_info("\t%llu \t %d \t %d\n", tmp.timestamp_ns, tmp.temp_mC, tmp.flags);
        bufferIndex = (bufferIndex+ 1) % TEMP_BUFFER_SIZE;
    }
}