#include <stdio.h>
#include <stdlib.h>
#include "amessage.pb-c.h"

int amessage_main (void)
{
	AMessage msg = AMESSAGE__INIT;	// AMessage
	void *buf;					// Buffer to store serialized data
	unsigned int len;				// Length of serialized data

	msg.a = 10;
    msg.has_b = 1;
    msg.b = 2;

	len = amessage__get_packed_size (&msg);

	buf = malloc (len);
	amessage__pack (&msg, buf);

	AMessage *msg_ptr;

  	// Unpack the message using protobuf-c.
  	msg_ptr = amessage__unpack(NULL, len, buf);
  	if (msg_ptr == NULL)
  	{
  		rt_kprintf("error unpacking incoming message\n");
  		exit(1);
  	}
  
  	// display the message's fields.
  	rt_kprintf("Received: a=%d",msg_ptr->a);  // required field
    if (msg_ptr->has_b)                   // handle optional field
    {
  		rt_kprintf("  b=%d",msg_ptr->b);
    }
    rt_kprintf("\n");
  
	free (buf);					// Free the allocated serialized buffer
	return 0;
}
