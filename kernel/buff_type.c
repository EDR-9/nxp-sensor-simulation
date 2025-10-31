/*

Attempt to write a functiona ring buffer knowing its elementary theory

*/

//#include "buff_type.h"
#include "nxp_simtemp.h"


void rbuffer_init( struct TempBuffer_t* buffer )
{
    /* Both buffer indexes start from zero */
    buffer->_head = 0;
    buffer->_tail = 0;
}

__u8 rbuffer_empty( struct TempBuffer_t* buffer )
{
    // If head = tail buffer must be really empty (no elements inside)
    
    if ( buffer->_head == buffer->_tail )
    {
        return 1;
    }
    else
    {
        return 0;
    }

    //return (buffer->_head == buffer->_tail);
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

    //return (buffer->_head + 1) % BUFFER_SIZE == buffer->_tail;
}

void rbuffer_enqueue( struct TempBuffer_t* buffer, struct simtemp_sample newTempSample /*, flag for reading when buffer is full (not accessed) */)
{
    if ( rbuffer_full(buffer) )
    {
        pr_info("Buffer has not been accessed, no element has been read ('dequeued')\n");
        // Instead of simple case exiting function, try to adda check asking for reading now

        // buffer->_samples[buffer->_tail] = newElement;
        // buffer->_tail = (buffer->_tail + 1) % BUFFER_SIZE;
    }
    else
    {
        /*
        Division: dividend = quotient*divisor + residue ---> residue = dividend - quotient*divisor
        n = (c + 1) mod s = (c + 1) - (s * (int)((c + 1)/s))
        c + 1 = n + ()
        */
        pr_info("Current head: %d\nReceived element at [%d]: %d\n", buffer->_head, buffer->_head, newTempSample.temp_mC);
        buffer->_samples[buffer->_head] = newTempSample;
        buffer->_head = (buffer->_head + 1) % TEMP_BUFFER_SIZE;
        pr_info("Next head: %d\n\n", buffer->_head);
    }
}

void rbuffer_dequeue( struct TempBuffer_t* buffer, struct simtemp_sample* extUserRead )
{
    if ( rbuffer_empty(buffer) )
    {
        pr_info("Buffer it's empty, there's no element to be read\n");
        // similar to enqueue function, try to add a check for adding a element here and now
    }
    else
    {
        pr_info("Current tail: %d\nRead element from [%d]: %d\n", buffer->_tail, buffer->_tail, buffer->_samples[buffer->_tail].temp_mC);
        *extUserRead = buffer->_samples[buffer->_tail];
        buffer->_tail = (buffer->_tail + 1) % TEMP_BUFFER_SIZE;
        pr_info("Next tail: %d\n\n", buffer->_tail);
    }
}

void rbuffer_show( struct TempBuffer_t* buffer, __u8 buffer_traversal_type )
{
    if ( buffer_traversal_type == TEMP_BUFFER_RAW_TRAVERSAL )
    {
        /* code */
    }
    else
    {
        pr_info("Buffer data:\n");
        __u8 index = buffer->_tail;
        while ( index != buffer->_head )
        {
            struct simtemp_sample tmp = buffer->_samples[index];
            pr_info("%llu \t %d \t %d\n", tmp.timestamp_ns, tmp.temp_mC, tmp.flags);
            index = (index + 1) % TEMP_BUFFER_SIZE;
            //index++;
        }
        //pr_info("\n");
    }
}


MODULE_LICENSE("GPL");


// void TestBufferFilling()
// {
//     TempBuffer_t testBuffer;
//     float readerSimulation;

//     rbuffer_init(&testBuffer);

//     /* Filling the buffer */
//     for (uint8_t i = 0; i < BUFFER_SIZE; i++)
//     {
//         rbuffer_enqueue(&testBuffer, 27.395137*(i+1));
//     }

//     rbuffer_show(&testBuffer, BUFFER_LOGICAL_TRAVERSAL);

//     pr_info("Current head: %d\nCurrent: %d\n", testBuffer._head, testBuffer._tail);

//     /* Reading some value from buffer (fake dequeue) */
//     rbuffer_dequeue(&testBuffer, &readerSimulation);
//     pr_info("Value read from buffer: %f\n", readerSimulation);
//     pr_info("Current head: %d\nCurrent: %d\n", testBuffer._head, testBuffer._tail);

//     rbuffer_show(&testBuffer, BUFFER_LOGICAL_TRAVERSAL);
// }

// void TestBufferBasicUsage()
// {
//     TempBuffer_t tempBuffer;
//     float externalReader;

//     /* Buffer initialization */
//     rbuffer_init(&tempBuffer);

//     /* Adding some values to the buffer */
//     for (uint8_t i = 0; i < (BUFFER_SIZE/4); i++)
//     {
//         rbuffer_enqueue(&tempBuffer, 3.141597*(i+1));
//     }

//     rbuffer_show(&tempBuffer, BUFFER_LOGICAL_TRAVERSAL);

//     /* Reading elements from the buffer */
//     rbuffer_dequeue(&tempBuffer, &externalReader);
//     pr_info("Value read from buffer: %f\n", externalReader);
//     rbuffer_dequeue(&tempBuffer, &externalReader);
//     pr_info("Another value from buffer: %f\n", externalReader);
//     pr_info("Read values must be exist in buffer: %f, %f", tempBuffer._samples[0], tempBuffer._samples[1]);

//     rbuffer_show(&tempBuffer, BUFFER_LOGICAL_TRAVERSAL);

//     /* Adding more elements to the buffer */
//     rbuffer_enqueue(&tempBuffer, 5.562357);
//     rbuffer_enqueue(&tempBuffer, 1.303972);

//     rbuffer_show(&tempBuffer, BUFFER_LOGICAL_TRAVERSAL);
// }



// int main()
// {
//     TestBufferFilling();

//     //TestBufferBasicUsage();

//     return 0;
// }