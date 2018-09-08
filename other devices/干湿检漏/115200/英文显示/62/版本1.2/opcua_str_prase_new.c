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
#include <fcntl.h> 	//文件控制定义
#include <termios.h>	//POSIX中断控制定�
#include <errno.h>	//错误号定义


#include "open62541.h"

#define serial_device "/dev/ttyS0"
#define STRING_LEN 30
#define OPCUA_SERVER_PORT 16664
#define	DEBUG_DATA
typedef struct _DATA_SOURCE {
	char* name;
	unsigned char type;   // 1 stand for string, 2 stand for float.
	float data;
	char string_data[STRING_LEN];
}DATA_SOURCE;

//函数申明
void OpenSreial(int *serial_fd);
int serialport();
void set_speed_and_parity(int fd);
int open_port(void);

UA_Server *g_opc_server;
#define YES 1
#define S_PORT 5888
#define R_PORT 5222
#define BACKLOG 7
int s_fd = 0;
int ret = 0;
int r_fd = 0;
int Serial_fd=0; //新加串口fd

struct argument{
	int fd;
	int port;
	int serverfd;
};
struct argument sendOriginalDataFD;
//struct argument recvDataFD;
DATA_SOURCE source[]={
		{"RecordNumber",1,0.0,{0}},
		{"Time",1,0.0,{0}},
		{"JobNumber",1,0.0,{0}},
		{"PieceNumber",1,0.0,{0}},
		{"TestPressure",1,0.0,{0}},
		{"LeakRate",1,0.0,{0}},
		{"Unit",1,0.0,{0}},
		{"Result",1,0.0,{0}}
	};

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

int uart_data_check(char *str_input)
{
	int i;
		for(i=1;i<=strlen(str_input)-3;i++)
		{
			if(str_input[i]<42 || str_input[i]>58)
			{
				printf("received data have illegal character!!!\n");
				return -1;
			}
		}
	if(str_input == NULL || strlen(str_input) > 50)
	{
		printf("received data's length is invalid!!!\n");
		return -1;
	}

	else
		printf("ok\n");
		return 1;
}

int praseStrToData(const char *str)
{
	int len=0,i;
	char *p;
	if(str == NULL) {
		return -1;
	}
	len = strlen(str);
	// check the data 
	if(uart_data_check(str) == 1) {
		//p = strstr(str,"#00 00");
		p=str;
		p += 1;
		if(p != NULL) {

			memcpy(source[0].string_data,p,4);			
			memcpy(source[1].string_data,p+4,12);
			memcpy(source[2].string_data,p+16,3);
			memcpy(source[3].string_data,p+19,8);
			memcpy(source[4].string_data+1,p+28,6);
			memcpy(source[5].string_data+1,p+35,7);
			memcpy(source[6].string_data,p+42,1);
			memcpy(source[7].string_data,p+43,1);
			
			if(*(p+27)=='0')
				{source[4].string_data[0]='+';}
			else if(*(p+27)=='1')
				{source[4].string_data[0]='-';}
			if(*(p+34)=='0')
				{source[5].string_data[0]='+';}
			else if(*(p+34)=='1')
				{source[5].string_data[0]='-';}
				
			source[0].string_data[4]='\0';
			source[1].string_data[12]='\0';
			source[2].string_data[3]='\0';
			source[3].string_data[8]='\0';
			source[4].string_data[7]='\0';
			source[5].string_data[8]='\0';
			source[6].string_data[1]='\0';
			source[7].string_data[1]='\0';
			}
		}
	else if(uart_data_check(str) == -1)
		{
			for(i=2;i<sizeof(source)/sizeof(DATA_SOURCE);i++)
				{
					memset(&source[i].string_data,'0',strlen(source[i].string_data));
				}
	}
#ifdef	DEBUG_DATA	
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
//创建服务器端
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

void OpenSreial(int *serial_fd){
	*serial_fd=serialport();	//包含打开串口及串口基本设置(波特率和数据位、停止位、有无校验等)
	if(*serial_fd==-1){
	perror("Can not open Serial_Port 1/n！");
		}
	else
	printf("open&set Serial_port 1 success!!!\n");
	while(1)
		{
		sleep(10);
	}
}

int serialport()
{
	int fd;	
	//打开串口
	if((fd=open_port())<0)
    	{
        	perror("open_port error!!!\n");
        	return -1;
    	}
	//设置波特率和校验位
	set_speed_and_parity(fd);
	return (fd);
}

int open_port(void)
{
	int fd;		//串口的标识符
	fd=open(serial_device,O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd == -1)
	{
		//不能打开串口
		perror("open_port: Unable to open /dev/ttyS0!!!\n");
		return(fd);
	}
	else
	{
		fcntl(fd, F_SETFL, 0);
		printf("open ttys0 .....\n");
		return(fd);
	}
}

void set_speed_and_parity(int fd)
{
	struct termios Opt;	//定义termios结构
	if(tcgetattr(fd,&Opt)!=0)
	{
		perror("tcgetattr fd");
		return;
	}
	tcflush(fd, TCIOFLUSH);
	cfsetispeed(&Opt, B115200);		//设置串口输入波特率
	cfsetospeed(&Opt, B115200);		//设置串口输出波特率
	if(tcsetattr(fd, TCSANOW, &Opt) != 0)	//保存测试现有串口参数设置，在这里如果串口号出错，会有相关的出错信息
	{	
		perror("tcsetattr fd");
		return;
	}
	tcflush(fd, TCIOFLUSH);
	
 	//设置奇偶校验——默认8个数据位、没有校验位       
    	Opt.c_cflag &= ~PARENB;		//清除校验位
   	  Opt.c_cflag &= ~CSTOPB;		//1个停止位
    	Opt.c_cflag &= ~CSIZE;		//Bit mask for data bits
    	Opt.c_cflag |= CS8;				//8个数据位
	
	//其他的一些配置
		Opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);			//原始输入，输入字符只是被原封不动的接收
		Opt.c_iflag &= ~(IXON | IXOFF | IXANY);							//软件流控制无效，因为硬件没有硬件流控制，所以就不需要管了
		Opt.c_oflag |= ~OPOST;											//原始输出方式可以通过在c_oflag中重置OPOST选项来选择：

//    	Opt.c_cflag &= ~INPCK;
   	Opt.c_cflag |= (CLOCAL | CREAD);
   	Opt.c_oflag &= ~(ONLCR | OCRNL);    
   	Opt.c_iflag &= ~(ICRNL | INLCR | IGNCR);    

	//VMIN可以指定读取的最小字符数。如果它被设置为0，那么VTIME值则会指定每个字符读取的等待时间。
    	Opt.c_cc[VTIME] = 1;	//设置超时为0sec
    	Opt.c_cc[VMIN] = 0;		//Update the Opt and do it now
    
    	tcflush(fd, TCIOFLUSH);//处理未接收字符
    if(tcsetattr(fd, TCSANOW, &Opt) != 0)	//激活新配置
	{	
		perror("com set error");
		return;
	}
	printf("set done!\n");
    	

}






//从串口读取数据
int ReadData(int fd,char *p){
	char c;
	int ret = 0;
	int num = 0;

	ret = read(fd, &c, 1);
	if(ret < 0){
		perror("recv");
		return 0;
	}
	else if(ret == 0){
		printf("接收数据出错\n");
		
		return -1;
	}
	else{
		
			*p = c;
			num = 1;
			while(1){
				ret = read(fd, p + num, 1);
				if(*(p + num) == '\n' ||*(p + num) == '\0'){//0x0D表示回车
					num++;
					return num;
				}else{
					num++;
				}
				if(num == 1000)
					return 0;
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
	fd_set readfd;
	int ret = 0;
	int ret_send = 0;
	int maxfd = 0;
	int num = 0;
	char *p;
	
//	printf("opcua adapter start, reviece port %d\n",R_PORT);
	printf("opcua adapter serial data start!!!\n");
	while(1){
		maxfd = 0;
		struct timeval timeout;
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;
		if(Serial_fd != 0){
			FD_ZERO(&readfd);
			FD_SET(Serial_fd, &readfd);
			maxfd = Serial_fd + 1;			
			ret = select(maxfd, &readfd, NULL, NULL, &timeout);
			if(ret == -1){
				perror("select");
			}else if(ret > 0 && FD_ISSET(Serial_fd, &readfd)){ //检查串口句柄是否还存在？
				//printf("ret = %d\n",ret);
				p = (char *)malloc(sizeof(char) * 1000);
				memset(p,0,sizeof(char)*1000);//初始化
				num = ReadData(Serial_fd, p);
				if(num > 0) {				
					printf("receive %d byte\n",num);
					printf("receive string is:%s\n",p);
					praseStrToData(p);
					if(SocketConnected(sendOriginalDataFD.fd)) {
					//if(sendOriginalDataFD.fd > 0) {
						ret_send = send(sendOriginalDataFD.fd, p, num, 0);
			
					printf("send string is:%s\n",p);
			
						if(ret_send < 0) {
							perror("send");
							close(sendOriginalDataFD.fd);
						}
						else if(ret_send == 0)
							printf("连接断开\n");
						else
							printf("write sendOriginalDataFD_fd! ret_send=%d\n",ret_send);
					}
				} else if (num == -1) {
					//client disconnect
					FD_CLR(Serial_fd, &readfd);
					printf("serial port has closed!!!\n");				
					//close(recvDataFD.fd);  //关闭socket连接   
		      		//Serial_fd=0;
				}
				free(p);
			}
		}
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

	//recvDataFD.port = R_PORT;
	sendOriginalDataFD.port = S_PORT;
	pthread_create(&original_data_outpit_id,NULL,(void *)creatserver,&sendOriginalDataFD);
	pthread_create(&r_id,NULL,(void *)OpenSreial,&Serial_fd);
	pthread_create(&source_Data_id,NULL,(void *)soureDataPrase,&Serial_fd);
//	sleep(1);
	pthread_create(&opcua_server_id,NULL,(void *)handle_opcua_server,NULL);
	
	while(1) {
		sleep(1);
	}
	return 0;
}
#if 0
int main()
{
	char  *data_input = "?id:1457049893000,url:S101_151123,maccode:QB99014,productname:S101-17-HVAC,jobnumber:1,evaporesi:2.04k,innerloop:162,outerloop:52,resvalueone:12.86,resvaluetwo:12.86,resvaluethree:12.86,resvaluefour:12.86,currvalueone:12.86,currvaluetwo:12.86,currvaluethree:12.86,currvaluefour:12.86,fanendpressone:12.86,fanendpresstwo:12.86,fanendpressthree:12.86,fanendpressfour:12.86,cudeproid:S1011607140569,checkresult:OK,nowtime:2016-7-14 11:49:25!";
	praseStrToData(data_input);
	printf("Hello World!\n");
    return 0;
}
#endif
