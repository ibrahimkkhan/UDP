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
#include <fcntl.h>
#include <time.h>
#include <iostream>
#include <map>  
#include <vector>
#include <arpa/inet.h> 
#include <inttypes.h>
using namespace std;
using std::cout;
using std::endl;


struct server_arguments {
	int port;
	int drop;
}__attribute__((packed));

struct time_request{
    uint32_t type = 0;
    uint16_t n = 0;
    uint64_t cli_secs = 0;
    uint64_t cli_nsecs = 0;
}__attribute__((packed));

struct time_response{
    uint32_t type = 0;
    uint16_t n = 0;
    uint64_t cli_secs = 0;
    uint64_t cli_nsecs = 0;
    uint64_t serv_secs = 0;
    uint64_t serv_nsecs = 0;
}__attribute__((packed));

struct client_info{
  time_t time;
  int maxSeq;
}__attribute__((packed));

int keepPayload(int probability){

	// srand((unsigned) time(NULL));
	int range = 100;
	int rand_num = rand() % (range + 1) + 0;
	if(rand_num > probability){
		return 1;
	}
	return -1;

}

void create_server(struct server_arguments *args){
	
    struct sockaddr_in servaddr, cliaddr;
	int sock_fd, n;
	socklen_t len;
	std::map<string, client_info> myMap;
	
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }else{
		printf("Socket created\n");
	}

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(args->port);

     if (bind(sock_fd, (const struct sockaddr *)&servaddr, 
            sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }else{
		printf("Server binded to Port %d and ADDR %s ..\n", args->port, args->drop,  inet_ntoa(servaddr.sin_addr));
	}

	
    len = sizeof(cliaddr); 
	int rec_cli = 0, keep_payload = 0;
    struct time_request req;
	
	void *time_buf = malloc(sizeof(struct time_request));
	struct timespec stamp;
	req.type = 0;
	req.n = 0;
    req.cli_secs = 0;
    req.cli_nsecs = 0;

	std::cout << "Wait..."<<endl;
    while(1){	

		int n;
		n = recvfrom(sock_fd, (void *) &req, sizeof(req), 0,
			(struct sockaddr *) &cliaddr, &len);
		if(n < 0){
			perror("Exit");
			exit(EXIT_FAILURE);
		}
		req.type = ntohl(req.type);
		req.n = ntohs(req.n);
		req.cli_secs = be64toh(req.cli_secs);
		req.cli_nsecs = be64toh(req.cli_nsecs);
			
		cout << "Received..."<< endl;
		if(rec_cli < 0){
				exit(EXIT_FAILURE);
			}
			keep_payload = keepPayload(args->drop);
			if(keep_payload > 0){
				clock_gettime(CLOCK_REALTIME, &stamp);
				unsigned int cli_port;
				char cli_IP[16];

				getsockname(sock_fd, (struct sockaddr *) &cliaddr, (socklen_t*)sizeof(cliaddr));
				inet_ntop(AF_INET, &(cliaddr.sin_addr), cli_IP, sizeof(cli_IP));
				cli_port = ntohs(cliaddr.sin_port);

				auto it = myMap.find(cli_IP); 

				if (it == myMap.end() ) { 
					myMap[cli_IP] = {(time_t)req.cli_secs, (int)req.type};
				}else{
					struct client_info req2 = myMap[cli_IP];
					if(req.type < req2.maxSeq){
						cout << inet_ntoa(cliaddr.sin_addr) << ":" << cliaddr.sin_port << req.type << req2.maxSeq << endl; 
					}

					if((req.cli_secs - req2.time) > 120){
						myMap.erase(it);
						struct client_info newReq = {(time_t)req.cli_secs, (int)req.type};
						myMap[cli_IP] = newReq;
					}
				}

				struct time_response resp;
				resp.type = htonl(req.type);
				resp.n = htons(req.n);
				resp.cli_secs = htobe64(req.cli_secs);
				resp.cli_nsecs = htobe64(req.cli_nsecs);
				resp.cli_nsecs = htobe64(req.cli_nsecs);
				resp.serv_nsecs = htobe64(stamp.tv_nsec);
				resp.serv_secs = htobe64(stamp.tv_sec);

				
				int n;
				n = sendto(sock_fd, &resp, sizeof(struct time_response),0, (const struct sockaddr *) &cliaddr, sizeof(struct sockaddr));
				if (n < 0){
					exit(0);
				}
				
			}else{
				continue;
			}

		}
}


error_t server_parser(int key, char *arg, struct argp_state *state) {
	struct server_arguments *args = (server_arguments* )(state->input);
	error_t ret = 0;

	switch(key) {
	case 'p':
		args->port = atoi(arg);
		if (args->port <= 1024) {
			argp_error(state, "Invalid option for a port, must be a number greater than 1024");
		}
		break;
	case 'd':
		args->drop = atoi(arg);
		if (args->drop < 0 || args->drop > 100) {
			argp_error(state, "Percentage of dropping payload should be in the range [0,100]");
		}
		break;
	default:
		ret = ARGP_ERR_UNKNOWN;
		break;
	}
	return ret;
}

int server_parseopt(int argc, char *argv[]) {
	struct server_arguments args;

	/* bzero ensures that "default" parameters are all zeroed out */
	bzero(&args, sizeof(args));



	struct argp_option options[] = {
		{ "port", 'p', "port", 0, "The port to be used for the server" ,0},
		{ "drop", 'd', "drop", 0, "The percentage chance that the server drops any given UDP payload", 0},
		{0}
	};
	struct argp argp_settings = { options, server_parser, 0, 0, 0, 0, 0 };
	if (argp_parse(&argp_settings, argc, argv, 0, NULL, &args) != 0) {
		printf("Got an error condition when parsing\n");
		return -1;
	}else{
		if(!args.port){
			printf("Port not specified!\n");
			exit(EXIT_FAILURE);

		}else{
			create_server(&args);
		}
	}
	return 0;
}



int main(int argc, char *argv[]) {
	return server_parseopt(argc, argv);
}