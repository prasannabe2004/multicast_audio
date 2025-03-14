#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <endian.h>

#define MULTICAST_ADDR "239.0.0.1"
#define PORT 12345
#define BUFFER_SIZE 4096

int main() {
    int sock;
    struct sockaddr_in multicast_addr;
    struct ip_mreq mreq;
    char buffer[BUFFER_SIZE];
    int rc;
    FILE *output_file;

    // Create socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Failed to create socket");
        exit(1);
    }

    // Set up multicast address
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    multicast_addr.sin_port = htons(PORT);

    // Bind socket
    if (bind(sock, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
        perror("Failed to bind socket");
        exit(1);
    }

    // Join multicast group
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_ADDR);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("Failed to join multicast group");
        exit(1);
    }

    // Open output file
    output_file = fopen("output.wav", "wb");
    if (output_file == NULL) {
        perror("Failed to open output file");
        exit(EXIT_FAILURE);
    }


    int total_bytes_received = 0;
    int bytes_received = 0;
    int packet_count = 0;

    // Receive and play audio data
    while (1) {
        bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            total_bytes_received += bytes_received;
            packet_count++;
            printf("Received packet %d, %d bytes (total: %d bytes)\n", packet_count, bytes_received, total_bytes_received);
            fwrite(buffer, bytes_received, 1, output_file);
        }
        else {
            break;
        }
    }
    fclose(output_file);
    close(sock);

    return 0;
}
