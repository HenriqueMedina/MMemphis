/*
 * global_mapper.c
 *
 *  Created on: Jul 1, 2019
 *      Author: ruaro
 */
#include "common_include.h"
#include "mapping_includes/global_mapper/cluster_controller.h"


#define 	MAX_MA_TASKS		(MAX_SDN_TASKS+MAX_MAPPING_TASKS)
#define		MAPPING_TASK_REPO_ID	1
#define		SDN_TASK_REPO_ID		2


int ma_tasks_location[MAX_MA_TASKS];

/*Common Useful functions*/
void send_task_allocation_message(unsigned int task_id, unsigned int allocated_proc, unsigned int master_addr){

	unsigned int * message;

	message = get_message_slot();
	message[0] = APP_INJECTOR; //Destination
	message[1] = 4; //Packet size
	message[2] = APP_ALLOCATION_REQUEST; //Service
	message[3] = task_id; //Repository task ID
	message[4] = master_addr; //Master address
	message[5] = allocated_proc;

	//Send message to Peripheral
	SendRaw(message, 6);

	while(!NoCSendFree());

	//Puts("Requesting task "); Puts(itoa(task_id)); Puts(" allocated at proc "); Puts(itoh(allocated_proc));
	//Puts(" belowing to master "); Puts(itoh(master_addr)); Puts("\n");

}

int get_local_mapper_index(unsigned int pos){
	int tx, ty;

	tx = pos >> 8;
	ty = pos & 0xFF;

	//Converts address in ID
	pos = (tx + ty*(XDIMENSION/MAPPING_XCLUSTER)); //PLus 1 because the global mapper uses ID 0

	return pos;
}

void map_management_tasks(){

	//Create an array with the tasks location.
	//Map task to PE
	//Fills the array
	//Format of task localtion  task_class | task_id  ||  proc
	int ma_task_index = 0;
	int cluster_index;
	int task_loc_aux;
	int x_aux, y_aux;
	int task_class, task_id;
	short int PE_usage[XDIMENSION][YDIMENSION];

	Puts("\nInitializing map_management_tasks\n\n");

	//Initializes the ma_tasks_location
	for(int i=0; i<MAX_MA_TASKS; i++){
		ma_tasks_location[i] = -1;
	}

	for(int y=0; y<YDIMENSION; y++){
		for(int x=0; x<XDIMENSION; x++){
			PE_usage[x][y] = MAX_LOCAL_TASKS;
		}
	}
	PE_usage[0][0] = 0;//Decrements because GM is allocated in this position

	ma_tasks_location[ma_task_index++] = 0;//The location of global mapper

	//**************************** Instantiate the Mapping MA tasks ********************************************
	Puts("\nStarting MAPPING MA instatiation\n");
	task_id = 0; //Initializes task ID
	task_class = 0;
	for(int y=0; y<MAPPING_Y_CLUSTER_NUM; y++){
		for(int x=0; x<MAPPING_X_CLUSTER_NUM; x++){

			x_aux = (x*MAPPING_XCLUSTER);
			y_aux = (y*MAPPING_YCLUSTER);

			//Puts("PE "); Puts(itoh(x_aux<<8|y_aux)); Puts(" has free resources of "); Puts(itoa(PE_usage[x_aux][y_aux])); Puts("\n");
			//Heuristic that tries to find a PE to the MA task
			while(PE_usage[x_aux][y_aux] < 1){
				for(int xi = x_aux; xi < (x_aux+MAPPING_XCLUSTER); xi++){
					for(int yi = y_aux; yi < (y_aux+MAPPING_YCLUSTER); yi++){
						//Puts("Trying PE "); Puts(itoh(xi<<8|yi)); Puts("\n");
						if (PE_usage[xi][yi] > 0){
							x_aux = xi;
							y_aux = yi;
							xi = (x_aux+MAPPING_XCLUSTER); //Breaks the first loop
							break;
						}
					}
				}
			}

			task_loc_aux = (x_aux << 8) | y_aux;
			//Request to the injector to load the local_mapper inside the PEs
			//Task ID of local mappers are always 1
			send_task_allocation_message(MAPPING_TASK_REPO_ID, task_loc_aux, 0);

			//Remove one resource from the selected PE
			PE_usage[x_aux][y_aux]--;

			//Allocate cluster resources
			cluster_index = get_cluster_index_from_PE(task_loc_aux);
			allocate_cluster_resource(cluster_index, 1);

			Puts("Task mapping id "); Puts(itoa(task_id)); Puts(" allocated at proc ");
			Puts(itoh(task_loc_aux)); Puts("\n");

			//Compose the full information of task location and stores into the array
			task_loc_aux = (task_class << 24) | (task_id << 16) | task_loc_aux;
			ma_tasks_location[ma_task_index++] = task_loc_aux;

			//Increments the task ID
			task_id++;

			//Decrements the total number of available resources of the system
			total_mpsoc_resources--;

		}
	}

	//**************************** Instantiate the SDN MA asks *****************************************
	Puts("\nStarting SDN MA instatiation\n");
	task_id = 0; //Initializes task ID
	task_class = 1;
	for(int y=0; y<SDN_Y_CLUSTER_NUM; y++){
		for(int x=0; x<SDN_X_CLUSTER_NUM; x++){

			x_aux = (x*SDN_XCLUSTER);
			y_aux = (y*SDN_YCLUSTER);

			//Heuristic that tries to find a PE to the MA task
			//Puts("PE "); Puts(itoh(x_aux<<8|y_aux)); Puts(" has free resources of "); Puts(itoa(PE_usage[x_aux][y_aux])); Puts("\n");
			while(PE_usage[x_aux][y_aux] < 1){
				for(int xi = x_aux; xi < (x_aux+SDN_XCLUSTER); xi++){
					for(int yi = y_aux; yi < (y_aux+SDN_YCLUSTER); yi++){
						//Puts("Trying PE "); Puts(itoh(xi<<8|yi)); Puts("\n");
						if (PE_usage[xi][yi] > 0){
							x_aux = xi;
							y_aux = yi;
							xi = (x_aux+SDN_XCLUSTER); //Breaks the first loop
							break;
						}
					}
				}
			}

			task_loc_aux = (x_aux << 8) | y_aux;
			//Request to the injector to load the local_mapper inside the PEs
			//Task ID of local mappers are always 1
			send_task_allocation_message(SDN_TASK_REPO_ID, task_loc_aux, 0);

			//Remove one resource from the selected PE
			PE_usage[x_aux][y_aux]--;

			//Allocate cluster resources
			cluster_index = get_cluster_index_from_PE(task_loc_aux);
			allocate_cluster_resource(cluster_index, 1);

			Puts("Task SDN id "); Puts(itoa(task_id)); Puts(" allocated at proc ");
			Puts(itoh(task_loc_aux)); Puts("\n");

			//Compose the full information of task location and stores into the array
			task_loc_aux = (task_class << 24) | (task_id << 16) | task_loc_aux;
			ma_tasks_location[ma_task_index++] = task_loc_aux;

			//Increments the task ID
			task_id++;

			//Decrements the total number of available resources of the system
			total_mpsoc_resources--;
		}
	}



	putsv("\ntotal_mpsoc_resources: ", total_mpsoc_resources);

	//Adiciona a si mesmo na tabela da task location
	AddTaskLocation(0, 0);

	//Verificar dimensoes do cluster
	//Requisitar app locais informando o seu ID no INIT

	for(int i=0; i<MAX_MA_TASKS;i++){
		Puts("Task MA "); Puts(itoa(ma_tasks_location[i] >> 16)); Puts(" allocated at "); Puts(itoh(ma_tasks_location[i] & 0xFFFF)); Puts("\n");
	}

	Puts("\nEnd map_management_tasks\n\n");
}


void handle_i_am_alive(unsigned int source_addr, unsigned int repo_id_class){
	static unsigned int received_i_am_alive_counter = 0;
	char error_flag = 1;
	unsigned int * message;
	unsigned int index = 0;

	Puts("handle_i_am_alive from "); Puts(itoa(source_addr)); Puts(" repo id class "); Puts(itoa(repo_id_class)); Puts("\n");


	received_i_am_alive_counter++;

	if (received_i_am_alive_counter == (MAX_MA_TASKS-1)){ //Minus 1 due the global mapper

		Puts("Initializing local mappers\n");
	}


	/*for(int i=0; i < CLUSTER_NUMBER; i++ ){
		if (clusters[i].free_resources == source_addr){

			//Adiciona o ID do cluster ao seu endereco verdadeiro
			//AddTaskLocation((i+1), source_addr);

			received_i_am_alive_counter++;

			error_flag = 0;
			break;
		}
	}

	//Puts("I am alive: "); Puts(itoa(received_i_am_alive_counter)); Puts("\n");

	while (error_flag)
		Puts("ERROR: received address does not belongs to any cluster\n");

	if (received_i_am_alive_counter == CLUSTER_NUMBER){

		Puts("Initializing local mappers\n");

		message = get_message_slot();
		message[0] = INIT_LOCAL;
		message[1] = 0; //Empty, will be filled for each thread
		message[2] = 0; //ID do global mapper
		message[3] = 0; //Address do global mapper
		index = 4;
		for(int i=0; i<CLUSTER_NUMBER; i++){
			message[index++] = (i+1); //Cluster task ID, +1 pq o global mapper possui o ID 0
			message[index++] = clusters[i].free_resources;
			//Puts("ID "); Puts(itoa((i+1)));
			//Puts("\naddr: "); Puts(itoh(clusters[i].free_resources)); Puts("\n");
		}

		for(int i=0; i<CLUSTER_NUMBER; i++){
			//Add task location temporally
			AddTaskLocation(1, clusters[i].free_resources);
			message[1] = (clusters[i].x_pos << 8 | clusters[i].y_pos);
			SendService(1, message, index);
		}

		Puts("Local Mappers Initialization completed!\n");

		//Update task location definitively
		for(int i=0; i<CLUSTER_NUMBER; i++){
			AddTaskLocation(i+1, clusters[i].free_resources);
			//From this moment this variable <free_resources> starts to store its real meaning
			if (i==0) //If the cluster has ID 0, remove two resources from its cluster
				clusters[i].free_resources = (MAX_LOCAL_TASKS * MAPPING_XCLUSTER * MAPPING_YCLUSTER) - 2;
			else
				clusters[i].free_resources = (MAX_LOCAL_TASKS * MAPPING_XCLUSTER * MAPPING_YCLUSTER) - 1;
		}*/

		/*Sending MAPPING COMPLETE to APP INJECTOR*/
		/*message = get_message_slot();
		message[0] = APP_INJECTOR;
		message[1] = 2;//Payload should be 1, but is 2 in order to turn around a corner case in traffic monitor of Deloream for packets with payload 1
		message[2] = APP_MAPPING_COMPLETE;
		SendRaw(message, 4);*/
	//}

}

void handle_new_app_req(unsigned int app_cluster_id, unsigned int app_task_number){

	static unsigned int app_id_counter = 1;
	unsigned int cluster_loc;
	unsigned int * message;

	if (app_cluster_id == -1) //This -1 comes from scripts, its means that mapping is dynamic
		app_cluster_id = CLUSTER_NUMBER;

	if (app_task_number > total_mpsoc_resources){
		pending_app_req = app_task_number << 16 | app_cluster_id;
		Puts("Cluster full - return\n");
		return;
	}

	pending_app_req = 0;

	Puts("\n\n******** NEW_APP_REQ **********\n");
	//Puts(itoa(app_cluster_id));
	putsv("Task number: ", app_task_number);

	if (app_cluster_id == CLUSTER_NUMBER){
		Puts("dynamic\n");
		app_cluster_id = search_cluster(app_task_number);
	} else {
		Puts("static\n");
	}

	Puts("App mapped at cluster "); Puts(itoh(  clusters[app_cluster_id].x_pos << 8 | clusters[app_cluster_id].y_pos )); Puts("\n");
	Puts("Application ID: "); Puts(itoa(app_id_counter)); Puts("\n");

	//putsv("Global Master reserve application: ", num_app_tasks);
	//putsv("total_mpsoc_resources ", total_mpsoc_resources);

	putsv("\nCURRENT - total_mpsoc_resources: ", total_mpsoc_resources);
	total_mpsoc_resources -= app_task_number;
	putsv("AFTER - total_mpsoc_resources: ", total_mpsoc_resources);

	cluster_loc = (app_cluster_id+1) << 16 | GetTaskLocation(app_cluster_id+1);

	//Puts("Cluster loc "); Puts(itoa(cluster_loc)); Puts("\n");

	/*Sends packet to APP INJECTOR requesting repository to cluster app_cluster_id*/
	message = get_message_slot();
	message[0] = APP_INJECTOR;
	message[1] = 3; //Payload size
	message[2] = APP_REQ_ACK;
	message[3] = app_id_counter;
	message[4] = cluster_loc; //PLus 1 because as the global mapper is in ID 0 it shift all other IDs in more 1
	SendRaw(message, 5);

	app_id_counter++;
}

void handle_app_terminated(unsigned int *msg){
	unsigned int task_master_addr, index, app_task_number;
	int borrowed_cluster_index, original_cluster, original_cluster_index;

	putsv("\n --- > Handle APP terminated- app ID: ", msg[1]);
	app_task_number = msg[2];
	original_cluster = msg[4];

	//Puts("original master pos: "); Puts(itoh(original_cluster)); Puts("\n");

	original_cluster_index = get_local_mapper_index(original_cluster);

	//Puts("original master index: "); Puts(itoa(original_cluster_index)); Puts("\n");

	index = 4;

	for (int i=0; i<app_task_number; i++){
		task_master_addr = msg[index++];

		//Puts("Terminated task id "); Puts(itoa(msg[1] << 8 | i)); Puts(" with master pos "); Puts(itoh(task_master_addr)); Puts("\n");

		if (task_master_addr != original_cluster){

			//Puts("Remove borrowed\n");
			borrowed_cluster_index = get_local_mapper_index(task_master_addr);

			release_cluster_resources(borrowed_cluster_index, 1);

		} else {
			//Puts("Remove original\n");
			release_cluster_resources(original_cluster_index, 1);
		}
	}
	total_mpsoc_resources += app_task_number;

	putsv("App terminated, total_mpsoc_resources: ", total_mpsoc_resources);

	/* terminated_app_count++;
	 * if (terminated_app_count == APP_NUMBER){
		Puts("FINISH ");Puts(itoa(GetTick)); Puts("\n");
		MemoryWrite(END_SIM,1);
	}*/
	//pending_app_req = app_task_number << 16 | app_cluster_id;
	if (pending_app_req){
		Puts("Pending APP to req TRUE\n");
		app_task_number = pending_app_req >> 16;
		original_cluster_index = pending_app_req & 0xFF;

		handle_new_app_req(original_cluster_index, app_task_number);

	}
}

void handle_app_allocated(unsigned int * msg){
	unsigned int cluster_index, index, app_task_number;
	unsigned int * message;

	putsv("\n******************************\nReceive APP_ALLOCATED for app ", msg[1]);
	app_task_number = msg[2];

	index = 3;
	for(int i=0; i<app_task_number; i++){
		cluster_index = get_local_mapper_index(msg[index++]);
		Puts("Task "); Puts(itoa(msg[1] << 8 | i)); Puts(" to cluster "); Puts(itoh(msg[index-1])); Puts("\n");
		allocate_cluster_resource(cluster_index, 1);
	}

	message = get_message_slot();
	message[0] = APP_INJECTOR;
	message[1] = 2; //Payload should be 1, but is 2 in order to turn around a corner case in traffic monitor of Deloream for packets with payload 1
	message[2] = APP_MAPPING_COMPLETE; //Service
	SendRaw(message, 4);
	Puts("Sent APP_MAPPING_COMPLETE\n");

}

void handle_message(unsigned int * data_msg){

	switch (data_msg[0]) {
		case INIT_I_AM_ALIVE:
			handle_i_am_alive(data_msg[1], data_msg[2]);
			break;
		case NEW_APP_REQ:
			handle_new_app_req(data_msg[1], data_msg[2]);
			break;
		case APP_ALLOCATED:
			handle_app_allocated(data_msg);
			break;
		case APP_TERMINATED:
			handle_app_terminated(data_msg);
			break;
		default:
			Puts("Error message unknown\n");
			for(;;);
			break;
	}
}

void main(){

	RequestServiceMode();

	Echo("Initializing Global Mapper\n");
	init_message_slots();
	intilize_clusters();
	map_management_tasks();
	//init_sdn_tasks();
	//init_dvfs_tasks();
	//init_monitoring_tasks();
	//init_decision_tasks();
	//init_adaptation_tasks();

	unsigned int data_message[MAX_MANAG_MSG_SIZE];

	for(;;){

		ReceiveService(data_message);
		handle_message(data_message);

	}

	exit();
}



