#ifndef  BUFFER_TYPE_H
#define  BUFFER_TYPE_H


#include "nxp_simtemp_main.h"



/* Functions for ring buffer implementation */
void rbuffer_init( struct TempBuffer_t* buffer );
__u8 rbuffer_empty( struct TempBuffer_t* buffer );
__u8 rbuffer_full( struct TempBuffer_t* buffer );
__u8 rbuffer_enqueue( struct TempBuffer_t* buffer, struct simtemp_sample* newTempSample );
__u8 rbuffer_dequeue( struct TempBuffer_t* buffer, struct simtemp_sample* extUserRead );
void rbuffer_show( struct TempBuffer_t* buffer, __u8 traversalType );



#endif