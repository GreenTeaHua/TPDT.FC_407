#include "hmc5883.h"
#include <stdio.h>
#include <math.h>  
#include <stdlib.h>
#include "ahrs.h"

//HMC5983��IICд��ַ
#define   SlaveAddress    0x3c           

#define	SDA2_Pin GPIO_Pin_11
#define	SCL2_Pin GPIO_Pin_10
#define delay_us(i) I2C_delay()
#define SAMPLE_COUNT 6
#define yaw_a 0.9
 
#define   IIC_SDA_1     GPIOB->BSRRL = SDA2_Pin     
#define   IIC_SDA_0     GPIOB->BSRRH  = SDA2_Pin    
#define   IIC_SCL_1     GPIOB->BSRRL = SCL2_Pin     
#define   IIC_SCL_0     GPIOB->BSRRH  = SCL2_Pin    
#define   READ_SDA	    GPIO_ReadInputDataBit(GPIOB,SDA2_Pin)   //��ȡSDA״̬
  

 
int Xmax_x,Xmin,Ymax_x,Ymin,Zmax_x,Zmin;     //X��Y ��Z ����Сֵ�����ֵ
int magOffsetX,magOffsetY,magOffsetZ;  //X��Y ��Z  ��ƫ����
u8 BUF[6]={0,0,0,0,0,0};               //���ڴ�Ŷ�ȡ��X��Y ��Z ֵ
float mag_angle;                          //�Ƕ�ֵ
int calThreshold=1;                    //ƫ�����Ƚ�ֵ

int mag_x=0,mag_y=0,mag_z=0;

static void I2C_delay(void)
{
    volatile int i = 30;
    while (i)
         i--;
}

//����IIC������Ϊ��� 
static void SDA_OUT(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    GPIO_InitStructure.GPIO_Pin = SDA2_Pin;
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}      

//����IIC������Ϊ����
static void SDA_IN(void)       
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    GPIO_InitStructure.GPIO_Pin = SDA2_Pin;
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_OType=GPIO_OType_OD;
    
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}   
  

//�����ƴ��������
static void HMC5883_GPIO_Config(void)
{
    GPIO_InitTypeDef  GPIO_InitStructure;
    
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB ,ENABLE);	   

	GPIO_InitStructure.GPIO_Pin =  SDA2_Pin | SCL2_Pin;	
 	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType=GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd=GPIO_PuPd_NOPULL;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	IIC_SDA_1;	  	  
	IIC_SCL_1;
}



void IIC_Start(void)
{
	SDA_OUT();     //sda�����
	IIC_SDA_1;	  	  
	IIC_SCL_1;
	delay_us(5);
 	IIC_SDA_0;//START:when CLK is high,DATA change form high to low 
	delay_us(5);
	IIC_SCL_0;//ǯסI2C���ߣ�׼�����ͻ�������� 
}



void IIC_Stop(void)
{
	SDA_OUT();
	IIC_SCL_0;
	IIC_SDA_0;
	delay_us(5);
	IIC_SCL_1;
	IIC_SDA_1;
	delay_us(5);
}


u8 IIC_Wait_Ack(void)
{
	u8 ucErrTime = 0;
	SDA_IN();
	IIC_SDA_1;
	delay_us(2);
	IIC_SCL_1;
	delay_us(2);
	while(READ_SDA)
	{
		ucErrTime++;
		if (ucErrTime > 250)
		{
			IIC_Stop();
			return 1;	  //Ӧ��ʧ��
		}
	}
	IIC_SCL_0;
	return 0;	//Ӧ��ɹ�
}



void IIC_Ack(void)                   
{
	IIC_SCL_0;
	SDA_OUT();
	IIC_SDA_0;
	delay_us(3);
	IIC_SCL_1;
	delay_us(3);
	IIC_SCL_0;
}

void IIC_NAck(void)                  
{
	IIC_SCL_0;
	SDA_OUT();
	IIC_SDA_1;
	delay_us(3);
	IIC_SCL_1;
	delay_us(3);
	IIC_SCL_0;
}

void IIC_Send_Byte(u8 txd)          //IIC����8λ����
{
	u8 t;

	SDA_OUT();      //SDA����Ϊ���
	IIC_SCL_0;    //SCLΪ��
	for (t = 0; t < 8; t++)
	{
// 		IIC_SDA = (tmag_xd&0mag_x80) >> 7;
        if(txd&0x80)
        {
            IIC_SDA_1;
        }
        else
        {
            IIC_SDA_0;
        }
        
		txd <<= 1;
		delay_us(3);
		IIC_SCL_1;
		delay_us(3);
		IIC_SCL_0;
	}
}

//��һ���ֽڣ�ack = 1ʱ�� ����ACK�� ack = 0������nACK
u8 IIC_Read_Byte(u8 ack)          //IIC ����8���ֽ�
{
	u8 i, receive = 0;

	SDA_IN();
	for (i = 0; i < 8; i++)
	{
		IIC_SCL_0;
		delay_us(4);
		IIC_SCL_1;
		receive <<= 1;
		if(READ_SDA) receive++;
		delay_us(3);
	}
	if (!ack)	IIC_NAck();
	else IIC_Ack();
	return receive;
}
 

void Write_HMC5983(u8 add, u8 da)      //HMC5983д���� 
{
    IIC_Start();                  //��ʼ�ź�
    IIC_Send_Byte(SlaveAddress);   //�����豸��ַ+д�ź�
		IIC_Wait_Ack();

    IIC_Send_Byte(add);    //�ڲ��Ĵ�����ַ����ο�����pdf 
		IIC_Wait_Ack();

    IIC_Send_Byte(da);       //�ڲ��Ĵ������ݣ���ο�����pdf
		IIC_Wait_Ack();

    IIC_Stop();                   //����ֹͣ�ź�
}


u8 Read_HMC5983(u8 REG_Address)         //HMC5983 ������
{   
		u8 REG_data;
    IIC_Start();                          //��ʼ�ź�
    IIC_Send_Byte(SlaveAddress);          //�����豸��ַ+д�ź�
		IIC_Wait_Ack();

    IIC_Send_Byte(REG_Address);           //���ʹ洢��Ԫ��ַ����0��ʼ	
		IIC_Wait_Ack();

    IIC_Start();                          //��ʼ�ź�
    IIC_Send_Byte(SlaveAddress+1);        //�����豸��ַ+���ź�
		IIC_Wait_Ack();

    REG_data=IIC_Read_Byte(0);             //�����Ĵ�������
		IIC_Stop();                           //ֹͣ�ź�
    return REG_data; 
}


//******************************************************
//
//��������HMC5983�ڲ��Ƕ����ݣ���ַ��Χ0x3~0x5
//
//******************************************************
void Multiple_read_HMC5983(u8*BUF)         //HMC5983 ��ȡһ�����ݡ���ȡ��ֵΪ X��Y��Z ��ֵ��
{  
    u8 i;
    IIC_Start();                          //��ʼ�ź�
    IIC_Send_Byte(SlaveAddress);           //�����豸��ַ+д�ź�
		IIC_Wait_Ack();
    IIC_Send_Byte(0x03);                   //���ʹ洢��Ԫ��ַ����0x3��ʼ	
		IIC_Wait_Ack();
    IIC_Start();                          //��ʼ�ź�
    IIC_Send_Byte(SlaveAddress+1);         //�����豸��ַ+���ź�
		IIC_Wait_Ack();
	for (i=0; i<6; i++)                      //������ȡ6����ַ���ݣ��洢��BUF
    { 
        if (i == 5)
        {
           BUF[i] = IIC_Read_Byte(0);          //���һ��������Ҫ��NOACK
        }
        else
        {
           BUF[i] = IIC_Read_Byte(1);          //����ACK
        }
    }
    IIC_Stop();                          //ֹͣ�ź�
}




void CollectDataItem(int magX, int magY, int magZ)     //����У׼HMC5983ʱ�á� У׼�ķ�����ͨ���ѴŸ�Ӧ��ԭ��ˮƽת��2Ȧ��
{
	if(magX > Xmax_x)       //�õ� X�����ֵ ����Сֵ 
		Xmax_x = magX;
	if(magX < Xmin)
		Xmin = magX;
		
	if(magY > Ymax_x)       //�õ� Y�����ֵ ����Сֵ 
		Ymax_x = magY;
	if (magY < Ymin)
		Ymin = magY;
		
	if(magZ > Zmax_x)       //�õ� Z�����ֵ ����Сֵ 
		Zmax_x = magZ;
	if (magZ < Zmin)
		Zmin = magZ;
	
	if(abs(Xmax_x - Xmin) > calThreshold)            //�����ֵ-��Сֵ /2 �õ�ƫ����
			magOffsetX = ( Xmax_x + Xmin) / 2; 
	if(abs(Ymax_x - Ymin) > calThreshold)
			magOffsetY = ( Ymax_x + Ymin) / 2;
	if(abs(Zmax_x - Zmin) > calThreshold)
			magOffsetZ = ( Zmax_x + Zmin) / 2;          //ƫ�������÷���  �ڽǶ������м�ȥƫ������
}

rt_thread_t hmc5883_thread;
struct rt_semaphore hmc5883_sem;
rt_bool_t has_hmc5883=RT_FALSE;
s16 hmc5883_avr[3][SAMPLE_COUNT]={0};

s16 mag;
FINSH_VAR_EXPORT(mag,finsh_type_short,mag angle of yaw);

void hmc5883_thread_entry(void* parameter)
{
	extern u8 balence;
	rt_kprintf("start hmc5883\n");
	
	while(1)
	{    
		u8 status=0;
		
		status = Read_HMC5983(0x09);  //��ȡstatus�Ĵ���
	 
		if((status & 0x01) == 0x01)   //���ݸ������
		{
			Multiple_read_HMC5983(BUF);
			
			mag_x = BUF[0] << 8 | BUF[1]; //Combine MSB and LSB of X Data output register
			mag_z = BUF[2] << 8 | BUF[3]; //Combine MSB and LSB of Z Data output register
			mag_y = BUF[4] << 8 | BUF[5]; //Combine MSB and LSB of Y Data output register
	  
			
			if(mag_x>32768)                 //�ѵõ���XYZֵ���д���
				mag_x = -(0xFFFF - mag_x + 1);
			if(mag_z>32768)
				mag_z = -(0xFFFF - mag_z + 1);
			if(mag_y>32768)
				mag_y = -(0xFFFF - mag_y + 1);	
			
			mag_x=MoveAve_WMA(mag_x,hmc5883_avr[0],SAMPLE_COUNT);
			mag_y=MoveAve_WMA(mag_y,hmc5883_avr[1],SAMPLE_COUNT);
			mag_z=MoveAve_WMA(mag_z,hmc5883_avr[2],SAMPLE_COUNT);
			
			if(mag_x>0&&mag_y>0)
			{
				 mag_angle= atan2((double)(mag_y),(double)(mag_x)) * (180 / 3.14159265);
			}
			else if(mag_x>0&&mag_y<0)
			{
				 mag_angle=360-atan2((double)(-mag_y),(double)(mag_x)) * (180 / 3.14159265);		
			}
			else if(mag_x<0&&mag_y<0)
			{
				 mag_angle=180+atan2((double)(-mag_y),(double)(-mag_x)) * (180 / 3.14159265);		
			}
			else if(mag_x<0&&mag_y>0)
			{
				 mag_angle=180-atan2((double)(mag_y),(double)(-mag_x)) * (180 / 3.14159265);		
			}
			else if(mag_x==0&&mag_y<0)
			{
				 mag_angle=270;		
			}
			else if(mag_x==0&&mag_y>0)
			{
				 mag_angle=90;		
			}		 
			else if(mag_x>0&&mag_y==0)
			{
				 mag_angle=0;		
			}
			else if(mag_x<0&&mag_y==0)
			{
				 mag_angle=180;		
			}
			if(ahrs.degree_yaw-mag_angle>360.0*yaw_a)
			{
				ahrs.degree_yaw=yaw_a*(ahrs.degree_yaw+ahrs.gryo_yaw/75.0)+(1.0-yaw_a)*(mag_angle+360.0);
				if(ahrs.degree_yaw>360.0)
					ahrs.degree_yaw-=360.0;
			}
			else if (ahrs.degree_yaw-mag_angle<-360.0*yaw_a)
			{
				ahrs.degree_yaw=yaw_a*(ahrs.degree_yaw+ahrs.gryo_yaw/75.0)+(1.0-yaw_a)*(mag_angle-360.0);
				if(ahrs.degree_yaw<0.0)
					ahrs.degree_yaw+=360.0;
			}
			else
				ahrs.degree_yaw=yaw_a*(ahrs.degree_yaw+ahrs.gryo_yaw/75.0)+(1.0-yaw_a)*mag_angle;
			mag=(s16)mag_angle;
			if(balence)
				rt_sem_release(&hmc5883_sem);
		}
		rt_thread_delay(RT_TICK_PER_SECOND/75);
	}
}
  
//�����Ƴ�ʼ��
rt_err_t HMC5983_Init(void)          //��ʼ��HMC5983
{
	u8 id;
	
	HMC5883_GPIO_Config();
    
	Write_HMC5983(0x00, 0x78);	 //�������Ƶ��Ϊ75HZ
 
	Write_HMC5983(0x02, 0x00);		//��������ģʽ
	
	id=Read_HMC5983(10);
	
	rt_kprintf("HMC5883:0x%x\n",id);
	
	rt_sem_init(&hmc5883_sem,"hmc_s",0,RT_IPC_FLAG_FIFO);
	
	if(id!=0x48)
	{
		has_hmc5883=RT_FALSE;
		rt_kprintf("HMC5883 not find\n");
		return RT_EEMPTY;
	}
	
	hmc5883_thread = rt_thread_create("hmc5883",hmc5883_thread_entry,RT_NULL,768,6,2);
	
	if(hmc5883_thread==RT_NULL)
	{
		has_hmc5883=RT_FALSE;
		rt_kprintf("HMC5883 thread no Memory\n");
		return RT_ENOMEM;
	}
	
	has_hmc5883=RT_TRUE;
	rt_thread_startup(hmc5883_thread);
	
	return RT_EOK;
}
 
