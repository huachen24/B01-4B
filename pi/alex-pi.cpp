#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdint.h>
#include "packet.h"
#include "serial.h"
#include "serialize.h"
#include "netconstants.h"
#include "constants.h"

#include "make_tls_server.h"
#include "tls_common_lib.h"


#define PORTNUM 		5000
#define KEY_FNAME   	"alex.key"
#define CERT_FNAME  	"alex.crt"
#define CA_CERT_FNAME   "signing.pem"
#define CLIENT_NAME     "laptop.epp.com"

#define PORT_NAME		"/dev/ttyACM0"
#define BAUD_RATE		B9600

sem_t _xmitSema;

char color = 0;
static volatile int networkActive=0;

void handleError(TResult error)
{
	switch(error)
	{
		case PACKET_BAD:
			printf("ERROR: Bad Magic Number\n");
			break;

		case PACKET_CHECKSUM_BAD:
			printf("ERROR: Bad checksum\n");
			break;

		default:
			printf("ERROR: UNKNOWN ERROR\n");
	}
}

void handleStatus(TPacket *packet)
{
	printf("\n ------- ALEX STATUS REPORT ------- \n\n");
	printf("Left Forward Ticks:\t\t%d\n", packet->params[0]);
	printf("Right Forward Ticks:\t\t%d\n", packet->params[1]);
	printf("Left Reverse Ticks:\t\t%d\n", packet->params[2]);
	printf("Right Reverse Ticks:\t\t%d\n", packet->params[3]);
	printf("Left Forward Ticks Turns:\t%d\n", packet->params[4]);
	printf("Right Forward Ticks Turns:\t%d\n", packet->params[5]);
	printf("Left Reverse Ticks Turns:\t%d\n", packet->params[6]);
	printf("Right Reverse Ticks Turns:\t%d\n", packet->params[7]);
	printf("Forward Distance:\t\t%d\n", packet->params[8]);
	printf("Reverse Distance:\t\t%d\n", packet->params[9]);
	printf("\n---------------------------------------\n\n");
}

void handleResponse(TPacket *packet)
{
	// The response code is stored in command
	switch(packet->command)
	{
		case RESP_OK:
			printf("Command OK\n");
		break;

		case RESP_STATUS:
			handleStatus(packet);
		break;

		default:
			printf("Arduino is confused\n");
	}
}

void handleErrorResponse(TPacket *packet)
{
	// The error code is returned in command
	switch(packet->command)
	{
		case RESP_BAD_PACKET:
			printf("Arduino received bad magic number\n");
		break;

		case RESP_BAD_CHECKSUM:
			printf("Arduino received bad checksum\n");
		break;

		case RESP_BAD_COMMAND:
			printf("Arduino received bad command\n");
		break;

		case RESP_BAD_RESPONSE:
			printf("Arduino received unexpected response\n");
		break;

		default:
			printf("Arduino reports a weird error\n");
	}
}

void handleMessage(TPacket *packet)
{
	printf("Message from Alex: %s\n", packet->data);
}

void handleColour(TPacket *packet)
{
	// The response code is stored in command
	switch(packet->command)
	{
		case COLOUR_GREEN:
			color = 'g';
			printf("Green detected.\n");
			break;

		case COLOUR_RED:
			color = 'r';
			printf("Red detected.\n");
			break;

		case COLOUR_NULL:
            color = 'n';
			printf("No colour detected.\n");
			break;
			
		default:
			printf("Corrupted colour data.\n");
	}
}

void handlePacket(TPacket *packet)
{
	switch(packet->packetType)
	{
		case PACKET_TYPE_COMMAND:
				// Only we send command packets, so ignore
			break;

		case PACKET_TYPE_RESPONSE:
				handleResponse(packet);
			break;

		case PACKET_TYPE_ERROR:
				handleErrorResponse(packet);
			break;

		case PACKET_TYPE_MESSAGE:
				handleMessage(packet);
			break;
		case PACKET_TYPE_COLOUR:
				handleColour(packet);
			break;
	}
}

void sendPacket(TPacket *packet)
{
	char buffer[PACKET_SIZE];
	int len = serialize(buffer, packet, sizeof(TPacket));

	serialWrite(buffer, len);
}

void *receiveThread(void *p)
{
	char buffer[PACKET_SIZE];
	int len;
	TPacket packet;
	TResult result;
	int counter=0;

	while(1)
	{
		len = serialRead(buffer);
		counter+=len;
		if(len > 0)
		{
			result = deserialize(buffer, len, &packet);

			if(result == PACKET_OK)
			{
				counter=0;
				handlePacket(&packet);
			}
			else 
				if(result != PACKET_INCOMPLETE)
				{
					printf("PACKET ERROR\n");
					handleError(result);
				}
		}
	}
}

void flushInput()
{
	char c;

	while((c = getchar()) != '\n' && c != EOF);
}

void sendCommand(char command, int32_t param1, int32_t param2)
{
	TPacket commandPacket;

	commandPacket.packetType = PACKET_TYPE_COMMAND;
	
	printf("command: %c\n", command);
	printf("param1: %d\n", param1);
	printf("param2: %d\n", param2);

	switch(command)
	{
		case 'w':
		case 'W':
			commandPacket.command = COMMAND_FORWARD;
			commandPacket.params[0] = param1;
			commandPacket.params[1] = param2;
			sendPacket(&commandPacket);
			break;

		case 'x':
		case 'X':
			commandPacket.command = COMMAND_REVERSE;
			commandPacket.params[0] = param1;
			commandPacket.params[1] = param2;
			sendPacket(&commandPacket);
			break;

		case 'a':
		case 'A':
			commandPacket.command = COMMAND_TURN_LEFT;
			commandPacket.params[0] = param1;
			commandPacket.params[1] = param2;
			sendPacket(&commandPacket);
			break;

		case 'd':
		case 'D':
			commandPacket.command = COMMAND_TURN_RIGHT;
			commandPacket.params[0] = param1;
			commandPacket.params[1] = param2;
			sendPacket(&commandPacket);
			break;

		case 's':
		case 'S':
			commandPacket.command = COMMAND_STOP;
			sendPacket(&commandPacket);
			break;

		case 'z':
		case 'Z':
			commandPacket.command = COMMAND_CLEAR_STATS;
			commandPacket.params[0] = 0;
			sendPacket(&commandPacket);
			break;

		case 'g':
		case 'G':
			commandPacket.command = COMMAND_GET_STATS;
			sendPacket(&commandPacket);
			break;

		case 'c':
		case 'C':
			commandPacket.command = COMMAND_GET_COLOUR;
			sendPacket(&commandPacket);
			break;
        case 'Q':
            system("sudo shutdown +1");
            killHandler(1);
		default:
			printf("Bad command\n");

	}
}

void sendData(void *conn, const char *buffer, int len)
{
    int c;
    printf("SENDING %d BYTES DATA\n", len);
    if(networkActive)
    {
        sslWrite(conn, buffer, len);
        networkActive = (c > 0);
    }
}

void *worker(void *conn) {
    int exit = 0;

    while(!exit) {

        int count;
        char buffer[128];

        count = sslRead(conn, buffer, sizeof(buffer));

        if(count > 0) {
            printf("I received %c, %d, %d", buffer[1], buffer[2], buffer[6]);
            sendCommand(buffer[1], buffer[2], buffer[6]);
            if (buffer[1] == 'c') {
                printf("waiting for colour data\n");
                while (!color){}
                printf("sending data now\n");
                memset(buffer, 0, sizeof(buffer));
                buffer[0] = NET_MESSAGE_PACKET;
                buffer[1] = color;
                count = sslWrite(conn, buffer, sizeof(buffer));
				color = 0;
				printf("sent successfully\n");
				if(count < 0) {
					perror("Error writing to network: ");
				}
			}
            memset(buffer, 0, sizeof(buffer));
            buffer[0] = RESP_OK;
            count = sslWrite(conn, buffer, sizeof(buffer));
            if(count < 0) {
                perror("Error writing to network: ");
            }
        }
        else if(count < 0) {
            perror("Error reading from network: ");
        }
    
		//printf("%d\n", count);
        // Exit of we have an error or the connection has closed.
        //exit = (count <= 0);
    }

    printf("\nConnection closed. Exiting.\n\n");
    killHandler(1);
}

int main()
{
	// Connect to the Arduino
	startSerial(PORT_NAME, BAUD_RATE, 8, 'N', 1, 5);

	// Sleep for two seconds
	printf("WAITING TWO SECONDS FOR ARDUINO TO REBOOT\n");
	sleep(2);
	printf("DONE\n");

	// Spawn receiver thread
	pthread_t recv;

	pthread_create(&recv, NULL, receiveThread, NULL);

	// Send a hello packet
	TPacket helloPacket;

	helloPacket.packetType = PACKET_TYPE_HELLO;
	sendPacket(&helloPacket);

	createServer(KEY_FNAME, CERT_FNAME, PORTNUM, &worker, CA_CERT_FNAME, CLIENT_NAME, 1);

	while(server_is_running())
	{
		//char ch;
		//printf("Command (f=forward, b=reverse, l=turn left, r=turn right, s=stop, c=clear stats, g=get stats q=exit)\n");
		//scanf("%c", &ch);

		//// Purge extraneous characters from input stream
		//flushInput();

		//sendCommand(ch);
	}

	printf("Closing connection to Arduino.\n");
	endSerial();
}
