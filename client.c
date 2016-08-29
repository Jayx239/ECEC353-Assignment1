/* Author: Naga Kandasamy
 *  * Last modified: 01/14/2014
 *   *
 *    *  Illustrates memory mapping and persistency, with POSIX objects.The client reads and displays a message left it in the 
 *     *  memory segment image by the server, a file been mapped from a memory segment.
 *      *
 *       *  Compile as follws: 
 *        *
 *         *  gcc -o client client_code.c -lrt
 *          *
 *           */

#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <time.h>
#include "data_types.h"
#include <signal.h>
#include <stdio.h>
#define SHARED_OBJECT_PATH         "/messenger"      
/* maximum length of the content of the message */
#define MAX_MESSAGE_LENGTH      50
/* how many types of messages we recognize */
#define MESSAGE_TYPES               8

int Connected = 0;
char user_message[MAX_MESSAGE_LEN];
char dm_rec[MAX_ID_LEN] = "foofoo";
int Keep_Alive;
pthread_mutex_t* user_input_mutex;
int MESSAGE_TYPE;

void open_connection(char uid[MAX_ID_LEN], msg_packet_t* shared_msg);

void close_connection(char uid[MAX_ID_LEN], msg_packet_t* shared_msg);
int send_message(msg_packet_t* shared_msg,char user_message[MAX_MESSAGE_LEN],char sender_id[MAX_ID_LEN], int MESSAGE_TYPE);
void* read_user_input(void* args);
void clean_id(char id[MAX_ID_LEN]);
void clean_exit(int dum);
/* message structure for messages in the shared segment */
/*struct msg_s {
  int type;
  char content[MAX_MESSAGE_LENGTH];
  };*/


int main(int argc, char *argv[]) {
	char Uid[MAX_ID_LEN];
	if(argc >1)
		strcpy(Uid,argv[1]);
	else
		strcpy(Uid,"foofag");

	Keep_Alive = 1;
	signal(SIGINT,clean_exit);
	int fd;
	int shared_seg_size = (sizeof(msg_packet_t));   /* We want a shared segment capable of storing one message */
	msg_packet_t* shared_msg;      /* The shared segment */


	/* Open the shared memory object using shm_open()  in read-only mode */
	fd = shm_open(SHARED_OBJECT_PATH, O_RDWR, S_IRWXU | S_IRWXG);
	if (fd < 0) {
		perror("In shm_open()");
		exit(1);
	}
	printf("Opened shared memory object %s\n", SHARED_OBJECT_PATH);

	/* requesting the shared segment using mmap() */    
	shared_msg = (struct msg_packet_t*)mmap(NULL, shared_seg_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (shared_msg == NULL) {
		perror("In mmap()");
		exit(1);
	}


	// Open Connection
	open_connection(Uid,shared_msg);
	pthread_t* user_input_thread;
	strcpy(user_message,"");

	pthread_mutex_init(&user_input_mutex,NULL);
	int rc = pthread_create(&user_input_thread,NULL,read_user_input,(void*) NULL);
	//	pthread_join(&user_input_thread,NULL);
	MESSAGE_TYPE = GROUP_MESSAGE;
	//Messaging code
	while(Keep_Alive)
	{
		// Listen for incoming messages
		pthread_mutex_lock(&shared_msg->mutex_lock);

		// direct message not setup, issue comparing user ids
		if(shared_msg->message_type == SERVER_MESSAGE)
		{
			//char rec_id[MAX_ID_LEN];
			//strcpy(rec_id,shared_msg->receiver_id);
			int match;
			match = strcmp(shared_msg->receiver_id,Uid);
			if(match == 0)
			{
				//	printf("%s %s\n",shared_msg->receiver_id, Uid);
				printf("%s: %s",shared_msg->sender_id,shared_msg->message);
				shared_msg->message_type = RESPONSE_MESSAGE;
			}
		}
		pthread_mutex_unlock(&shared_msg->mutex_lock);

		// Read user input and send message when completely entered
		pthread_mutex_lock(&user_input_mutex);
		if(strcmp(user_message,"") != 0)
		{
			if(send_message(shared_msg,user_message,Uid, MESSAGE_TYPE) == 1)
				strcpy(user_message,"");

		} 
		pthread_mutex_unlock(&user_input_mutex);
	}

	// Close connection
	close_connection(Uid,shared_msg);


	return 0;
}


void open_connection(char Uid[MAX_ID_LEN], msg_packet_t* shared_msg)
{
	int is_connected;
	is_connected  = CONNECT;
	while(is_connected != CONNECTED && Keep_Alive)
	{
		pthread_mutex_lock(&shared_msg->mutex_lock);
		if(shared_msg->message_type != NULL_MESSAGE)
		{
			pthread_mutex_unlock(&shared_msg->mutex_lock);
			continue;
		}
		shared_msg->message_type = SERVER_MESSAGE;
		strcpy(shared_msg->sender_id, Uid);
		strcpy(shared_msg->receiver_id,"");
		shared_msg->connection = CONNECT;
		is_connected = CONNECTED;
		pthread_mutex_unlock(&shared_msg->mutex_lock);
	}

}

void close_connection(char Uid[MAX_ID_LEN], msg_packet_t* shared_msg)
{
	int is_connected;
	is_connected = CONNECTED;
	while(is_connected == CONNECTED)
	{
		pthread_mutex_lock(&shared_msg->mutex_lock);
		if(shared_msg->message_type != NULL_MESSAGE)
		{
			pthread_mutex_unlock(&shared_msg->mutex_lock);
			continue;
		}
		shared_msg->message_type = SERVER_MESSAGE;
		strcpy(shared_msg->sender_id, Uid);
		strcpy(shared_msg->receiver_id,"");
		shared_msg->connection = DISCONNECT;
		is_connected = DISCONNECT;
		pthread_mutex_unlock(&shared_msg->mutex_lock);

		if(!Keep_Alive)
			break;
	}

}

int send_message(msg_packet_t* shared_msg,char user_message[MAX_MESSAGE_LEN],char sender_id[MAX_ID_LEN], int MESSAGE_TYPE)
{
	int msg_type;
	msg_type  = SERVER_MESSAGE;
	while(Keep_Alive)
	{
		pthread_mutex_lock(&shared_msg->mutex_lock);
		if(shared_msg->message_type == NULL_MESSAGE)
		{
			//shared_msg->message_type = MESSAGE_TYPE;
			strcpy(shared_msg->receiver_id,dm_rec);
			strcpy(shared_msg->sender_id,sender_id);
			strncpy(shared_msg->message,user_message,MAX_MESSAGE_LEN);
			shared_msg->message_type = MESSAGE_TYPE;
			pthread_mutex_unlock(&shared_msg->mutex_lock);
			return 1;
		}
		if(shared_msg->message_type == SERVER_MESSAGE)// && strcmp(shared_msg->receiver_id,sender_id) == 0)
		{
			pthread_mutex_unlock(&shared_msg->mutex_lock);
			return 0;
		}
		//printf("waiting");
		pthread_mutex_unlock(&shared_msg->mutex_lock);
	}	

}

void* read_user_input(void* args)
{
	int user_message_set;
	char* rec;
	rec = (char*) malloc(sizeof(char)* MAX_ID_LEN);
	//MESSAGE_TYPE = GROUP_MESSAGE;
	while(Keep_Alive)
	{
		char temp_message[MAX_MESSAGE_LEN];
		fgets(temp_message,MAX_MESSAGE_LEN,stdin);
		if(strncmp(temp_message,EXIT_COMMAND,EXIT_COMMAND_LEN) == 0)//temp_message[0]== '-' && temp_message[1] == 'e')
			break;
		if(strncmp(temp_message,DIRECT_MESSAGE_COMMAND,DIRECT_MESSAGE_COMMAND_LEN) == 0)//temp_message[0] == '-' && temp_message[1] == 'd' && temp_message[2] == 'm')
		{
			//	MESSAGE_TYPE = DIRECT_MESSAGE;
			printf("Enter recipient for direct message: ");
			gets(rec);
			//fgets(dm_rec, MAX_ID_LEN,stdin);
			//clean_id(dm_rec);
			//printf("client send rec: %s", rec);
			//MESSAGE_TYPE = DIRECT_MESSAGE;
			printf("Enter Message: ");
			fgets(temp_message,MAX_MESSAGE_LEN,stdin);

			MESSAGE_TYPE = DIRECT_MESSAGE;
		}
		else
		{
			MESSAGE_TYPE = GROUP_MESSAGE;
			// strcpy(dm_rec,"");
		}
		user_message_set = 1;
		while(user_message_set && Keep_Alive){
			pthread_mutex_lock(&user_input_mutex);
			if(strcmp(user_message,"") == 0)
			{
				strcpy(&dm_rec,rec);
				strcpy(user_message,temp_message);
				user_message_set = 0;
			}
			pthread_mutex_unlock(&user_input_mutex);
		}
	}
	Keep_Alive = 0;
}

void clean_id(char id[MAX_ID_LEN])
{
	int i;
	for(i=0; i<MAX_ID_LEN; i++)
	{
		printf("%s\n",id[i]);
		if(id[i] == '\n' || id[i] == '\t')
		{
			id[i] = '\0';
			break;
		}
	}
	id[MAX_ID_LEN-1] = '\0';

}

void clean_exit(int dum)
{
	Keep_Alive = 0;
}

