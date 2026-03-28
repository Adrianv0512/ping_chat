#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

//identifies our messages vs normal pings
#define PC_MAGIC     0xC0DE 
#define PC_MSG_TEXT  0x01  
#define MAX_PAYLOAD  1400  

#pragma pack(push, 1)

struct icmp_header {
    uint8_t  type; 
    uint8_t  code; 
    uint16_t checksum;
    uint16_t id;      
    uint16_t seq;     
};

struct pc_header {
    uint16_t magic;      
    uint8_t  msg_type;   
    uint16_t seq_num;    
    uint16_t frag_count; 
    uint16_t frag_index; 
    uint16_t payload_len;
};

#pragma pack(pop)

//AI GENERATED CHECKSUM ALGORITHM
/* -----------------------------------------------------------------------
 * compute_checksum — RFC 1071 internet checksum.
 *
 * Sum every 16-bit word in [data, data+len).  If len is odd the trailing
 * byte is zero-padded to a full 16-bit word.  Fold the 32-bit accumulator
 * down to 16 bits, then return the one's complement.
 *
 * IMPORTANT: the caller must zero the checksum field in the ICMP header
 * BEFORE passing the buffer to this function, then write the return value
 * back into that field.
 * --------------------------------------------------------------------- */
static inline uint16_t compute_checksum(const void *data, int len)
{
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        uint16_t last = 0;
        *(uint8_t *)&last = *(const uint8_t *)ptr;
        sum += last;
    }

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return (uint16_t)(~sum);
}

#endif
