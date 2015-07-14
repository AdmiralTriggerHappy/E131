/*
* E131.h
*
* Project: E131 - E.131 (sACN) library for Arduino
* Copyright (c) 2015 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#ifndef _E131_h
#define _E131_h

#include "Arduino.h"
#include "util.h"

/* ESP8266 detection */
#if defined (ESP8266)
#   define _UDP WiFiUDP
#   include <ESP8266WiFi.h>
#   include <ESP8266WiFiMulti.h>
#   include <WiFiUdp.h>
#else
#   define _UDP EthernetUDP
#   define _ETHERNET_
#   include <Ethernet.h>
#   include <EthernetUdp.h>
#endif

/* E1.31 Defaults */
#define E131_DEF_PORT 5568

/* E1.31 Packet Offsets */
#define E131_ROOT_PREAMBLE_SIZE 0
#define E131_ROOT_POSTAMBLE_SIZE 2
#define E131_ROOT_ID 4
#define E131_ROOT_FLENGTH 16
#define E131_ROOT_VECTOR 18
#define E131_ROOT_CID 22

#define E131_FRAME_FLENGTH 38
#define E131_FRAME_VECTOR 40
#define E131_FRAME_SOURCE 44
#define E131_FRAME_PRIORITY 108
#define E131_FRAME_RESERVED 109
#define E131_FRAME_SEQ 111
#define E131_FRAME_OPT 112
#define E131_FRAME_UNIVERSE 113

#define E131_DMP_FLENGTH 115
#define E131_DMP_VECTOR 117
#define E131_DMP_TYPE 118
#define E131_DMP_ADDR_FIRST 119
#define E131_DMP_ADDR_INC 121
#define E131_DMP_COUNT 123
#define E131_DMP_DATA 125

/* E1.31 Packet Structure */
typedef union {
    struct {
        /* Root Layer */
        uint16_t preamble_size;
        uint16_t postamble_size;
        byte	 acn_id[12];
        uint16_t root_flength;
        uint32_t root_vector;
        byte     cid[16];

        /* Frame Layer */
        uint16_t frame_flength;
        uint32_t frame_vector;
        byte     source_name[64];
        uint8_t  priority;
        uint16_t reserved;
        uint8_t  sequence_number;
        uint8_t  options;
        uint16_t universe;

        /* DMP Layer */
        uint16_t dmp_flength;
        uint8_t  dmp_vector;
        uint8_t  type;
        uint16_t first_address;
        uint16_t address_increment;
        uint16_t property_value_count;
        byte     property_values[513];
	} __attribute__((packed));

    byte raw[638];
} e131_packet_t;

typedef struct {
    uint32_t    num_packets;
    uint32_t    sequence_errors;
} e131_stats_t;

/* Error Types */
typedef enum {
    ERROR_NONE,
    ERROR_ACN_ID,
    ERROR_PACKET_SIZE,
    ERROR_VECTOR_ROOT,
    ERROR_VECTOR_FRAME,
    ERROR_VECTOR_DMP
} e131_error_t;

class E131 {
	private:
        /* Constants for packet validation */
        static const byte ACN_ID[];
        static const uint32_t VECTOR_ROOT = 4;
        static const uint32_t VECTOR_FRAME = 2;
        static const uint8_t VECTOR_DMP = 2;
        
        e131_packet_t pbuff1, pbuff2;   /* Packet buffers */
        e131_packet_t *pwbuff;          /* Pointer to working packet buffer */
        uint8_t       sequence;         /* Sequence tracker */
        _UDP udp;                       /* UDP handle */

        /* Internal Initializers */
        void initWiFi(const char *ssid, const char *passphrase);
        void initEthernet();
        void initUnicast(uint16_t port);
        void initMulticast(uint16_t universe);

	public:
        byte          *data;        /* Pointer to DMX channel data from last valid packet */
        uint16_t      universe = 0; /* DMX Universe of last valid packet */
        e131_packet_t *packet;      /* Pointer to last valid packet */
        e131_stats_t  stats;        /* Statistics tracker */

        E131();

        /* Unicast WiFi Initializers */
        int begin(const char *ssid);
        int begin(const char *ssid, uint16_t port);
        int begin(const char *ssid, const char *passphrase);
        int begin(const char *ssid, const char *passphrase, uint16_t port);

        /* Multicast WiFi Initializers  -- ESP8266 */
        int beginMulticast(const char *ssid, uint16_t universe);
        int beginMulticast(const char *ssid, const char *passphrase, uint16_t universe);

        //TODO - add Ethernet Initializers

        /* Diag functions */
        void dumpError(e131_error_t error);

        /* Main packet parser */
        inline uint16_t parsePacket() {
            e131_error_t error;
            uint16_t retval = 0;

            int size = udp.parsePacket();
            if (size) {
                udp.readBytes(pwbuff->raw, size);
                error = validate();
                if (!error) {
                    e131_packet_t *swap = packet;
                    packet = pwbuff;
                    pwbuff = swap;
                    universe = htons(packet->universe);
                    data = packet->property_values + 1;
                    retval = htons(packet->property_value_count) - 1;
                    if (packet->sequence_number != sequence++) {
                        stats.sequence_errors++;
                        sequence = packet->sequence_number + 1;
                    }
                    stats.num_packets++;
                } else {
                    dumpError(error);
                }
            }
            return retval;
        }

        /* Packet validater */
        inline e131_error_t validate() {
            if (memcmp(pwbuff->acn_id, ACN_ID, sizeof(pwbuff->acn_id)))
                return ERROR_ACN_ID;
            if (htonl(pwbuff->root_vector) != VECTOR_ROOT)
                return ERROR_VECTOR_ROOT;
            if (htonl(pwbuff->frame_vector) != VECTOR_FRAME)
                return ERROR_VECTOR_FRAME;
            if (pwbuff->dmp_vector != VECTOR_DMP)
                return ERROR_VECTOR_DMP;
            return ERROR_NONE;
        }
};

#endif
