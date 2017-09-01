#include <stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>

#define	NUM		10

int main(int argc,char **argv)
{
	//    int *content,*buffer;
	//    content=(int*)malloc(100*sizeof(int)); 
	//    buffer =(int*)malloc(100*sizeof(int));
      
	int i,fd,cnt;
	char key = 0;
	char runFlag = 1;
	char Send_buf[NUM] = {0};
	char Recv_buf[NUM] = {0};

	fd=open("/dev/rb_dev",O_RDWR);
	if(fd < 0)	    
		printf("rb_dev: open failed=%d!\n",fd);
	else
		printf("rb_dev: open success,fd=%d!\n",fd);


    while(runFlag)
    {
		printf("\n-------------------------------------------------------------------------------------------\n");
		printf(" 0:Exit 1:Recv_WriteACK 2: Recv_Read 3:Send_Write 4: Send_ReadACK -------------------------\n");
		key = getchar();
        getchar();
		//printf("%c\n",key);

		switch(key)	
		{
			//Exit
			case '0':
			{
				runFlag = 0;
				break;
			}
		
			//Recv_Write: ACK: 1byte = 0x50
			case '1':
			{
				printf("1-Recv_Write_ACK: start\n");

				Send_buf[0]=0x55; 
				Send_buf[1]=0x55; 
				Send_buf[2]=0x55; 
				cnt = write(fd,Send_buf,3);      //此处write是以byte为单位的

				if(cnt != 3)
					printf("1-Recv_Write_ACK: failed\n");
				else
					printf("1-Recv_Write_ACK: OK\n");  

				break;
			}

			//Recv_Read: DATA: 10bytes: 0x30--0x39
			case '2':
			{
				printf("2-Recv_Read_DATA: start\n");

				cnt = read(fd,Recv_buf,NUM);			

				if(cnt != NUM )
				{
					printf("2-Recv_Read_DATA: failed,cnt=%d\n",cnt);
				}
				else
				{
					printf("2-Recv_Read_DATA: OK,cnt=%d\n",cnt);

					for(i=0;i<cnt;i++)  
					{
						printf("%x\t",*(Recv_buf+i));
						if((i+1)%10==0)
						printf("\n");
					}
				}	
			
				break;
			}
#if 0	
			//Send_Write:DATA: 10*n bytes
			case '3':
			{
				printf("3-Send_Write_DATA: start\n");

				for(i=0;i<NUM;i++)   //初始化
					Send_buf[i]=0x30+i;  

				cnt = write(fd,Send_buf,NUM);      //此处write是以byte为单位的

				if(cnt != NUM )
					printf("3-Send_Write_DATA: failed\n");
				else
					printf("3-Send_Write_DATA: OK\n");  

				break;
			}

			//Send_Read: ACK: 1byte = 0x50
			case '4':
			{
				printf("4-Send_Read_ACK: start\n");

				cnt = read(fd,Recv_buf,1);			

				if(cnt != 1 )
				{
					printf("4-Send_Read_ACK: failed,cnt=%d\n",cnt);
				}
				else
				{
					if(Recv_buf[0] == 0x50 )
						printf("4-Send_Read_ACK: OK,cnt=%d\n",cnt);
					else
						printf("4-Send_Read_ACK: DATA error,cnt=%d\n",cnt);

					printf("%x\n",Recv_buf[0]);
				}	
				break;
			}
#endif

			default:
				break;

		}//end switch

   }//end while()

	// close device
    close(fd);

    return 0;
}
