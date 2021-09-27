#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <argp.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include<cstdlib>
#include <arpa/inet.h> 
#include <inttypes.h>
#include <math.h>  
#include <vector>
#include <sys/poll.h>
#include <time.h>    
#include <iostream>
#include <fstream> 
using namespace std;


#define PORT 2000


struct client_arguments {
	char ip_address[16]; 
	int port; 
	int timereqs;
	int timout;
} __attribute__((packed));

struct time_request{
    uint32_t type;
    uint16_t n;
    uint64_t cli_secs;
    uint64_t cli_nsecs;
}__attribute__((packed));

struct output_client{
	int seq;
    long double delta;
	long double theta;

	output_client(){
		seq = -1;
		delta = INT_MIN	;
		theta = INT_MIN	;
	};
}__attribute__((packed));

struct time_response{
    uint32_t type;
    uint16_t n;
    uint64_t cli_secs;
    uint64_t cli_nsecs;
    uint64_t serv_secs;
    uint64_t serv_nsecs;
}__attribute__((packed));

void accept_time_resp(struct client_arguments *args, int sockfd, int numReqs){
	int i = 0;
	struct timeval tv;
	
	struct pollfd fds[1];
	fds[0].fd= sockfd;
	fds[0].events = POLLIN;
	int s = (int)numReqs;
	std::vector<output_client> arr(numReqs);
	int temp= 0;

	while(i < numReqs){
		//cout << numReqs << endl;

		if(args->timout == 0){
			temp = -1;
		}
		else{
			temp = (args->timout)*1000;
		}

		if(poll(fds, 1, temp) == 0){
			break;
		}

		struct time_response resp;
		struct timespec end_stamp;
		clock_gettime(CLOCK_REALTIME, &end_stamp); 
		setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

		int n = recvfrom(sockfd, &resp, sizeof(resp), 0, NULL, 0);

		resp.type = ntohl(resp.type);
        resp.n = ntohs(resp.n);
        long double client_secs = be64toh((long double)resp.cli_secs);
        long double client_nsecs = ((long double) be64toh(resp.cli_nsecs))/1000000000;
        long double server_nsecs = ((long double) be64toh(resp.serv_nsecs))/1000000000;
        long double server_secs = be64toh((long double)resp.serv_secs);
		long double curr_nsec = ((long double)end_stamp.tv_nsec)/1000000000;
		

		long double delta_sec = ((long double)end_stamp.tv_sec - client_secs);
		long double delta_nsec = (curr_nsec- client_nsecs);
		long double delta = delta_sec + delta_nsec;

		long double theta1 = (server_secs - client_secs) + (server_nsecs - client_nsecs);
		long double theta2 = (server_secs - (long double)end_stamp.tv_sec) + (server_nsecs - curr_nsec);
		long double theta = (theta1 + theta2)/2;

		struct output_client c;
		c.theta = theta;
		c.delta = delta;
		c.seq = resp.type;

		arr.insert((arr.begin() + resp.type) - 1, c);
		
		i++;
	}

	int j;
	struct output_client c;
	for(j = 0; j < numReqs; j++){
		c = arr.at(j);
		if(c.seq == -1 || c.theta == INT_MIN || c.delta == INT_MIN ){
			std::cout << j+1 <<  ": Dropped" <<endl;

		}else{
			printf("%d: %.4Lf %.4Lf\n", c.seq, c.theta, c.delta);
		}
	}

}



void send_time_req(int sockfd, int numReqs, struct sockaddr_in servaddr){
	int i = 0;
	int n;
	struct timespec stamp[numReqs];
	printf("Client binded to ADDR %s ..\n", inet_ntoa(servaddr.sin_addr));


	while(i < numReqs){
		struct time_request req;
		clock_gettime(CLOCK_REALTIME, &stamp[i]); 
		req.type = htonl(i + 1);
		req.n = htons(7);
		req.cli_secs = htobe64(stamp[i].tv_sec);
		req.cli_nsecs = htobe64(stamp[i].tv_nsec);


        n = sendto(sockfd, (void *) &req, sizeof(req), 0, ( struct sockaddr *) &servaddr, sizeof(servaddr));
        if (n < 0) {
        	exit(0);
        }

		i++;
	}

}


void start_client(struct client_arguments *args){
	int sockfd, n, len;
	struct sockaddr_in servaddr;

	 if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }else{
		printf("Socket created...");
	}

	memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
   	servaddr.sin_addr.s_addr = inet_addr(args->ip_address);
    servaddr.sin_port = htons(args->port);
	send_time_req(sockfd, args->timereqs, servaddr);
	accept_time_resp(args, sockfd, args->timereqs);
	close(sockfd);
}


error_t client_parser(int key, char *arg, struct argp_state *state) {
	struct client_arguments *args = (client_arguments *) state->input;
	error_t ret = 0;
	int len;
	switch(key) {
	case 'a':
		strncpy(args->ip_address, arg, 16);
		if(strlen(args->ip_address) == 0){
			argp_error(state, "Enter address");
		}else if (0 /* ip address is goofytown */) {
			argp_error(state, "Invalid address");
		}
		
		break;
	case 'p':
		args->port = atoi(arg);
		if (args->port <= 1024){
			argp_error(state, "Invalid option for a port, must be a number");
		}
		break;
	case 'n':
		if(atoi(arg) < 0){
			argp_error(state, "Invalid option for a TimeRequests, must be a number greater than equal to 0");
		}else{
			args->timereqs = atoi(arg);
		}
		break;
	case 't':
		if(atoi(arg) < 0){
			argp_error(state, "Invalid option for a Timeout, must be a number greater than equal to 0");
		}else{
			args->timout = atoi(arg);
		}
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	

	return ret;
}


void client_parseopt(int argc, char *argv[]) {
	struct argp_option options[] = {
		{ "addr", 'a', "addr", 0, "The IP address the server is listening at", 0},
		{ "port", 'p', "port", 0, "The port that is being used at the server", 0},
		{ "timereqs", 'n', "timereqs", 0, "The number of Time requests to send to the server", 0},
		{ "timout", 't', "timout", 0, "The Timeout seconds to send to the server", 0},
		{0}
	};

	struct argp argp_settings = { options, client_parser, 0, 0, 0, 0, 0};

	struct client_arguments args;
	bzero(&args, sizeof(args));

	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got error in parse\n");
	}else{
		if(!args.port || !args.timereqs){
			printf("Port or time requests (n) not specified! You must specify both port & number of time requests! \n");
			exit(EXIT_FAILURE);
		}else{
			start_client(&args);
		}
	}
	
	printf("Got %s on port %d with n=%d t=%d\n",
	       args.ip_address, args.port, args.timereqs, args.timout);
}


int main(int argc, char *argv[]) {
	client_parseopt(argc, argv); //parse options as the client would
}
