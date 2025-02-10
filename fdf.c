#include <stdio.h>  
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>

// Pseudo header needed for TCP checksum calculation
struct pseudo_header {
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

// Checksum function
unsigned short csum(unsigned short *ptr, int nbytes) {
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
    return(answer);
}

int main(void) {
    int s = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);  // Create raw socket
    if (s == -1) {
        perror("Failed to create socket");
        exit(1);
    }

    char datagram[4096], source_ip[32], *data, *pseudogram;

    memset(datagram, 0, 4096);  // Zero out the packet buffer

    struct iphdr *iph = (struct iphdr *)datagram;  // IP header
    struct tcphdr *tcph = (struct tcphdr *)(datagram + sizeof(struct ip));  // TCP header
    struct sockaddr_in sin;
    struct pseudo_header psh;

    data = datagram + sizeof(struct iphdr) + sizeof(struct tcphdr);
    strcpy(data, "Hello, this is the payload!");  // Your custom data

    strcpy(source_ip, "11.111.1.2");  // Source IP (host machine)
    sin.sin_family = AF_INET;
    sin.sin_port = htons(1234);  // Port number (target port on the container)
    sin.sin_addr.s_addr = inet_addr("172.17.0.2");  // Destination IP (Docker container IP)

    // Fill in the IP Header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + strlen(data);
    iph->id = htonl(54321);  // ID of this packet
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;  // Set to 0 before calculating checksum
    iph->saddr = inet_addr(source_ip);  // Source IP (host machine)
    iph->daddr = sin.sin_addr.s_addr;
    // IP checksum
    iph->check = csum((unsigned short *)datagram, iph->tot_len);

    // TCP Header for SYN
    tcph->source = htons(1234);  // Source port
    tcph->dest = htons(1234);    // Destination port
    tcph->seq = 0;               // Initial sequence number
    tcph->ack_seq = 0;           // ACK number for SYN
    tcph->doff = 5;              // TCP header size
    tcph->fin = 0;
    tcph->syn = 1;               // Set SYN flag for initiating the handshake
    tcph->rst = 0;
    tcph->psh = 0;
    tcph->ack = 0;
    tcph->urg = 0;
    tcph->window = htons(5840);  // Maximum allowed window size
    tcph->check = 0;             // Leave checksum 0 for now, will be calculated later
    tcph->urg_ptr = 0;

    // Pseudo header for TCP checksum calculation
    psh.source_address = inet_addr(source_ip);
    psh.dest_address = sin.sin_addr.s_addr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr) + strlen(data));

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + strlen(data);
    pseudogram = malloc(psize);

    memcpy(pseudogram, (char *)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr) + strlen(data));

    // Calculate checksum for TCP
    tcph->check = csum((unsigned short*)pseudogram, psize);

    // Set IP_HDRINCL to tell the kernel that headers are included in the packet
    int one = 1;
    const int *val = &one;
    if (setsockopt(s, IPPROTO_IP, IP_HDRINCL, val, sizeof(one)) < 0) {
        perror("Error setting IP_HDRINCL");
        exit(0);
    }

    // Send the SYN packet to initiate the connection
    if (sendto(s, datagram, iph->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("sendto failed");
    } else {
        printf("SYN packet sent\n");
    }

    // Now wait for the SYN-ACK from the target and send the ACK packet
    // Normally you would use a socket library like `recv` to listen for the SYN-ACK,
    // but since this is raw socket programming, you'd need to implement the handling.

    // Send the actual data (ACK packet) after the handshake
    tcph->syn = 0;     // Remove SYN flag
    tcph->ack = 1;     // Set ACK flag
    tcph->seq = 1;     // Sequence number after handshake
    tcph->ack_seq = 1; // Acknowledgement number (sequence number of the other side)

    // Update the data
    strcpy(data, "This is data after the handshake.");

    // Recalculate the checksum for the new packet
    psh.tcp_length = htons(sizeof(struct tcphdr) + strlen(data));
    memcpy(pseudogram, (char *)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr) + strlen(data));

    // TCP checksum for ACK packet with data
    tcph->check = csum((unsigned short*)pseudogram, psize);

    // Send the ACK packet with the data
    if (sendto(s, datagram, iph->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("sendto failed");
    } else {
        printf("Data packet sent after handshake\n");
    }

    return 0;
}




