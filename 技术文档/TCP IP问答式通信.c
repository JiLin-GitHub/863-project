#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <linux/tcp.h>

#include "open62541.h"

#define STRING_LEN 30
#define OPCUA_SERVER_PORT 16664
typedef struct _DATA_SOURCE {
	char* name;
	unsigned char type;   // 1 stand for string, 2 stand for float.
	float data;
	char string_data[STRING_LEN];
}DATA_SOURCE;

UA_Server *g_opc_server;
#define YES 1
#define S_PORT 5888
#define R_PORT 5222
#define BACKLOG 7

/************增加****************/
#define PROT_FD  0
#define DEV_FD  1
#define TCP_PRESET_REQ_LENGTH 7

/************增加****************/
int s_fd = 0;
int ret = 0;
int r_fd = 0;

struct argument{
	int fd;
	int port;
	int serverfd;
};
struct argument sendOriginalDataFD;
struct argument recvSendDataFD;
DATA_SOURCE source[]={
	{"jobnumber",2,0.0,{0}},
	{"evaporesi",2,0.0,{0}},
	{"innerloop",2,0.0,{0}},
	{"outerloop",2,0.0,{0}},
	{"resvalueone",2,0.0,{0}},
	{"resvaluetwo",2,0.0,{0}}
};
int getDataFromStr( char* const str, DATA_SOURCE *source) 
{
	char *p=NULL;
	int i;
	p = strstr(str,source->name);
	if(p != NULL) {
		p = strstr(p,":");
			if(p != NULL) {
				p += 1;  //skip ":"
				if(source->type == 1) {
					for(i=0; i < STRING_LEN - 1;i++) {
						source->string_data[i] = p[i];
						if(p[i] == ',' || p[i] == '!') {
							source->string_data[i] = '\0';
							break;
						}
					}					
				} else if(source->type == 2) {
					source->data = atof(p);
				} else {
					return -1;
				}
				return 1;
			} else
				return -1;
	} else 
		return -1;
}

void debug_data_source()
{
	int i;	
	printf("variable----Type----Data\n");
	for(i=0;i<sizeof(source)/sizeof(DATA_SOURCE);i++) {
		if(source[i].type == 1)
			printf("%s     %d      %s\n", source[i].name,source[i].type,source[i].string_data);
		else if(source[i].type == 2)
			printf("%s     %d      %f\n", source[i].name,source[i].type,source[i].data); 			
	}
}
int praseStrToData(char *str)
{
	int len=0,i; 
	if(str == NULL) {
		return -1;
	}
	len = strlen(str);
	//检查首尾
	/*
	if(*str != '?' || str[len-1] != '!') {
		return -1;
	}*/
	if(len < 0 && len >1000)
		return -1;
	for(i=0;i<sizeof(source)/sizeof(DATA_SOURCE);i++) {
		getDataFromStr(str,(DATA_SOURCE*)&source[i]);
	}
	#ifdef DEBUG
		debug_data_source();
	#endif
	return 1;
}



void creat_server_sockfd4(int *sockfd, struct sockaddr_in *local, int portnum){
	int err;
	int optval = YES;
	int nodelay = YES;

	*sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(*sockfd < 0){
		perror("socket");
		exit(EXIT_FAILURE);
	}
	err = setsockopt(*sockfd,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
	if(err){
		perror("setsockopt");
	}
	err = setsockopt(*sockfd,IPPROTO_TCP,TCP_NODELAY,&nodelay,sizeof(nodelay));
	if(err){
		perror("setsockopt");
	}


	memset(local, 0, sizeof(struct sockaddr_in));
	local->sin_family = AF_INET;
	local->sin_addr.s_addr = htonl(INADDR_ANY);
	local->sin_port = htons(portnum);

	err = bind(*sockfd, (struct sockaddr*)local, sizeof(struct sockaddr_in));
	if(err < 0){
		perror("bind");
		exit(EXIT_FAILURE);
	}
	err = listen(*sockfd, BACKLOG);
	if(err < 0){
		perror("listen");
		exit(EXIT_FAILURE);
	}

}
//鍒涘缓鏈嶅姟鍣?
void creatserver(struct argument *p){
	char addrstr[100];
	int serverfd;
	struct sockaddr_in local_addr_s;
	struct sockaddr_in from;
	unsigned int len = sizeof(from);

	creat_server_sockfd4(&serverfd,&local_addr_s,p->port);

	while(1)
	{
		p->fd = accept(serverfd, (struct sockaddr*)&from, &len);
		if(ret == -1){
			perror("accept");
			exit(EXIT_FAILURE);
		}
		struct timeval time;
		gettimeofday(&time, NULL);
		printf("time:%lds, %ldus\n",time.tv_sec,time.tv_usec);
		printf("a IPv4 client from:%s\n",inet_ntop(AF_INET, &(from.sin_addr), addrstr, INET_ADDRSTRLEN));
	}

}

/* Builds a RTU request header */
static int TCP_build_request_basis( int protol_fd, char device_fd, char function, char reg_num,unsigned char *req )
{
    req[0] = protol_fd >> 8;
    req[1] = protol_fd & 0x00ff;
    req[2] = device_fd;
    req[3] = function;
    req[4] = 0;
    req[5] = 0;
    req[6] = reg_num;
    
    return TCP_PRESET_REQ_LENGTH;
}

/*
int serial_232_send_msg_pre(unsigned char *req, int req_length)
{
    unsigned int crc = (unsigned int)crc16check(req, req_length);
    req[req_length++] = crc >> 8;
    req[req_length++] = crc & 0x00FF;
    return req_length;
}
*/

//读数据
int ReadData(int fd,char *p){
	char c;
	int ret = 0;
	int num = 0;

	ret = recv(fd, &c, 1,0);
	if(ret < 0){
		perror("recv");
		return 0;
	}
	else if(ret == 0){
		printf("无数据接收\n");
		
		return -1;
	}
	else{

			*p = c;
			num = 1;
			while(1){
				ret = recv(fd, p + num, 1, 0);
				if (ret < 0) 
					{
						perror("readerr");
						return 0;
					}
					else if (ret = 0) 
						{
							return -1；
						}
				num++;
				if(num == 588)
					return num;
			}
	}	
}

int SocketConnected(int sock) 
{ 
	if(sock<=0) 
		return 0; 
	struct tcp_info info; 
	int len=sizeof(info); 
	getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *)&len); 
	//if((info.tcpi_state==TCP_ESTABLISHED))	{
	if((info.tcpi_state==1))	{
//		printf("socket connected\n"); 
		return 1; 
	} 
	else 	{ 
//		printf("socket disconnected\n"); 
		return 0; 
	} 
}


void* soureDataPrase(void *arg)
{
	int ret_send = 0;
	int num = 0;
	int iRec=0;
	int i;
  unsigned char *p;
	sleep(1);
//	printf("opcua adapter start, reviece port %d\n",R_PORT);
	printf("opcua adapter serial data start!!!\n");
	while(1){
		
		if(SocketConnected(recvSendDataFD.fd)){
		
				unsigned char *frame_buf;
				int length;
				frame_buf=(unsigned char*)malloc(sizeof(unsigned char)*20);
				memset(frame_buf,0,20);
				length=TCP_build_request_basis(PROT_FD,DEV_FD,4,23,frame_buf);
//				length=serial_232_send_msg_pre(buf,length);
				
				iRec=send(recvSendDataFD.fd,frame_buf,length, 0);
				if(iRec < 0) {
							perror("send");
							close(recvSendDataFD.fd);
						}
						else if(ret_send == 0)
							printf("连接断开\n");
						else 
								{
									printf("send message is:");
									for(i=0;i<length;i++)
										{
											printf("%x ",buf[i]);
										}
									printf("\n");
									
									p = (unsigned char *)malloc(sizeof(unsigned char) * 1000);
									memset(p,0,sizeof(p));//初始化
									
									num = ReadData(recvSendDataFD, p);									
									if(num > 0) {
									printf("receive %d byte\n",num);
									printf("received data is:");
									for(i=0;i<num;i++)
									{
										printf("%x ",p[i]);
									}
									printf("\n");
									praseStrToData(p);	
									
						   		if(SocketConnected(sendOriginalDataFD.fd)) {
										//if(sendOriginalDataFD.fd > 0) {
											ret_send = send(sendOriginalDataFD.fd,NewData,datalen_t, 0);						
											if(ret_send < 0) {
												perror("send");
												close(sendOriginalDataFD.fd);
											}
											else if(ret_send == 0)
												printf("连接断开\n");
											else
												printf("write sendOriginalDataFD_fd! ret_send=%d\n",ret_send);
												printf("The New String is:%s\n",NewData);
									}																		
								}			
							 else if (num == -1) {
							//client disconnect
							printf("Lan port has not receiced data!!!\n");				
							//close(recvDataFD.fd);  //关闭socket连接   
				      		//Serial_fd=0;
								}
				free(p);
				}							
				free(frame_buf);				
		}
			sleep(20);	
	}
}

#include <signal.h>

UA_Boolean running = true;
static void stopHandler(int sig) {    
	running = false;
}

void* nodeidFindData(const UA_NodeId nodeId) 
{
	int i;
	for(i=0;i<sizeof(source)/sizeof(DATA_SOURCE);i++) {
		if(strncmp((char*)nodeId.identifier.string.data, source[i].name, strlen(source[i].name)) == 0) {
			if(source[i].type == 1) {
				return &source[i].string_data[0];
			}
			else if(source[i].type == 2) {
				return (float*)&source[i].data;
			}
			
		}			
	}
	printf("not find:%s!\n",nodeId.identifier.string.data);
	return NULL;
}

static UA_StatusCode
readFloatDataSource(void *handle, const UA_NodeId nodeId, UA_Boolean sourceTimeStamp,
                const UA_NumericRange *range, UA_DataValue *value) {
    if(range) {
        value->hasStatus = true;
        value->status = UA_STATUSCODE_BADINDEXRANGEINVALID;
        return UA_STATUSCODE_GOOD;
    }		
	UA_Float currentFloat;

	if(nodeidFindData(nodeId) != NULL)
		currentFloat = *(UA_Float*)nodeidFindData(nodeId);
	else 
		currentFloat = -1;
	value->sourceTimestamp = UA_DateTime_now();
	value->hasSourceTimestamp = true;
    UA_Variant_setScalarCopy(&value->value, &currentFloat, &UA_TYPES[UA_TYPES_FLOAT]);
	value->hasValue = true;
	return UA_STATUSCODE_GOOD;
}

static UA_StatusCode
readStringDataSource(void *handle, const UA_NodeId nodeId, UA_Boolean sourceTimeStamp,
                const UA_NumericRange *range, UA_DataValue *value) {
    if(range) {
        value->hasStatus = true;
        value->status = UA_STATUSCODE_BADINDEXRANGEINVALID;
        return UA_STATUSCODE_GOOD;
    }		
	UA_String tempString;
	UA_String_init(&tempString);
	
	tempString.length = strlen(nodeidFindData(nodeId));
	tempString.data = nodeidFindData(nodeId);
	value->sourceTimestamp = UA_DateTime_now();
	value->hasSourceTimestamp = true;
    UA_Variant_setScalarCopy(&value->value, &tempString, &UA_TYPES[UA_TYPES_STRING]);
	value->hasValue = true;
	return UA_STATUSCODE_GOOD;
}


void add_dataSource_to_opcServer()
{
	int i;
	for(i=0;i<sizeof(source)/sizeof(DATA_SOURCE);i++) {
		if(source[i].type == 1) {
			//printf("%s     %d      %s\n", source[i].name,source[i].type,source[i].string_data);
			UA_DataSource dateDataSource = (UA_DataSource) {.handle = NULL, .read = readStringDataSource, .write = NULL};
			UA_VariableAttributes *attr_string = UA_VariableAttributes_new();
    	UA_VariableAttributes_init(attr_string);
			
			UA_String *init_string = UA_String_new();
			UA_String_init(init_string);
				
			UA_Variant_setScalar(&attr_string->value, init_string, &UA_TYPES[UA_TYPES_STRING]);
			attr_string->description = UA_LOCALIZEDTEXT("en_US",source[i].name);
	    attr_string->displayName = UA_LOCALIZEDTEXT("en_US",source[i].name);
			attr_string->accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, source[i].name);
	    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, source[i].name);
	    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
	    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
//	    UA_Server_addVariableNode(g_opc_server, myIntegerNodeId, parentNodeId,
//	                              parentReferenceNodeId, myIntegerName,
//	                              UA_NODEID_NULL, *attr, NULL, NULL);
		  UA_Server_addDataSourceVariableNode(g_opc_server, myIntegerNodeId,parentNodeId,
	                              					parentReferenceNodeId, myIntegerName,
                                                UA_NODEID_NULL, *attr_string, dateDataSource, NULL);
		}
		else if(source[i].type == 2) {
			UA_DataSource dateDataSource = (UA_DataSource) {.handle = NULL, .read = readFloatDataSource, .write = NULL};
			UA_VariableAttributes *attr = UA_VariableAttributes_new();
    	UA_VariableAttributes_init(attr);
			UA_Float floatData = source[i].data;
			UA_Variant_setScalar(&attr->value, &floatData, &UA_TYPES[UA_TYPES_FLOAT]);
			attr->description = UA_LOCALIZEDTEXT("en_US",source[i].name);
	    attr->displayName = UA_LOCALIZEDTEXT("en_US",source[i].name);
			attr->accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
	    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, source[i].name);
	    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, source[i].name);
	    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
	    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
//	    UA_Server_addVariableNode(g_opc_server, myIntegerNodeId, parentNodeId,
//	                              parentReferenceNodeId, myIntegerName,
//	                              UA_NODEID_NULL, *attr, NULL, NULL);
		  UA_Server_addDataSourceVariableNode(g_opc_server, myIntegerNodeId,parentNodeId,
	                              					parentReferenceNodeId, myIntegerName,
                                                UA_NODEID_NULL, *attr, dateDataSource, NULL);


			
		}
			//printf("%s     %d      %f\n", source[i].name,source[i].type,source[i].data); 			
	}	
}
void handle_opcua_server(void * arg){
		//signal(SIGINT,  stopHandler);
    //signal(SIGTERM, stopHandler);

    UA_ServerConfig config = UA_ServerConfig_standard;
    UA_ServerNetworkLayer nl = UA_ServerNetworkLayerTCP(UA_ConnectionConfig_standard, OPCUA_SERVER_PORT);
    config.networkLayers = &nl;
    config.networkLayersSize = 1;
    g_opc_server = UA_Server_new(config);

		/* add a variable node to the address space */
    UA_VariableAttributes attr;
    UA_VariableAttributes_init(&attr);
    UA_Float myInteger = 0.42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_FLOAT]);
    attr.description = UA_LOCALIZEDTEXT("en_US","the answer");
    attr.displayName = UA_LOCALIZEDTEXT("en_US","the answer");
    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, "the.answer");
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "the answer");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(g_opc_server, myIntegerNodeId, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NULL, attr, NULL, NULL);
		add_dataSource_to_opcServer();
    UA_Server_run(g_opc_server, &running);
		
    UA_Server_delete(g_opc_server);
    nl.deleteMembers(&nl);  
}

int main(){

	pthread_t r_id;
	pthread_t source_Data_id;
	pthread_t original_data_outpit_id;
	pthread_t opcua_server_id;

	recvSendDataFD.port = R_PORT;
	sendOriginalDataFD.port = S_PORT;
	pthread_create(&original_data_outpit_id,NULL,(void *)creatserver,&sendOriginalDataFD);
	pthread_create(&r_id,NULL,(void *)creatserver,&recvSendDataFD);
	pthread_create(&source_Data_id,NULL,(void *)soureDataPrase,&recvDataFD);
//	sleep(1);
	pthread_create(&opcua_server_id,NULL,(void *)handle_opcua_server,NULL);
	
	while(1) {
		sleep(1);
	}
	return 0;
}

