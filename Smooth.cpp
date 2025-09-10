#include <iostream>
#include <string>
#include <fstream>
#include <stdio.h>
#include <cstring>
#include <cstdlib>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include "bmp.h"

using namespace std;

#define NSmooth 1000

BMPHEADER bmpHeader;                        
BMPINFO bmpInfo;
RGBTRIPLE **BMPSaveData = NULL;                                               
RGBTRIPLE **BMPData = NULL;                                                   
RGBTRIPLE **individual_data=NULL;

int thread_num=0,counter=0;
sem_t count_sem;
sem_t barrier_sem[2];

int readBMP( char *fileName);        //read file
int saveBMP( char *fileName);        //save file
void swap(RGBTRIPLE *a, RGBTRIPLE *b);
RGBTRIPLE **alloc_memory( int Y, int X );        //allocate memory
void* smooth(void*);

int main(int argc,char *argv[])
{
	
	struct timeval start,end;


	char *infileName = "input.bmp";
	char *outfileName = "output.bmp";
	
	thread_num=atoi(argv[1]);	//get thread number
	sem_init(&count_sem,0,1);	//initial semaphore
	for(int i=0;i<2;i++){
		sem_init(&barrier_sem[i],0,0);
	}

        if ( readBMP( infileName) )
                cout << "Read file successfully!!" << endl;
        else 
                cout << "Read file fails!!" << endl;
	//initial needed memory
        BMPData = alloc_memory( bmpInfo.biHeight+2, bmpInfo.biWidth);
	individual_data=alloc_memory(bmpInfo.biHeight+2,bmpInfo.biWidth);
	//duplicate the graph to the data which used by thread
	for(int i=0;i<bmpInfo.biHeight;i++){
		for(int j=0;j<bmpInfo.biWidth;j++){
			BMPData[i+1][j]=BMPSaveData[i][j];
			individual_data[i+1][j]=BMPSaveData[i][j];
		}
	}
	//fill the buffer in head and tail of the matrix
	for(int i=0;i<bmpInfo.biWidth;i++){
		BMPData[0][i]=BMPData[bmpInfo.biHeight][i];
		BMPData[bmpInfo.biHeight+1][i]=BMPData[1][i];
	}
	int cal_range[thread_num*2]={};
	int avg=bmpInfo.biHeight/thread_num,remain=bmpInfo.biHeight%thread_num;
	//caculate the range
	for(int i=0;i<thread_num;i++){
		cal_range[i*2]=i*avg;
		cal_range[i*2+1]=cal_range[i*2]+avg;
	}
	pthread_t p[thread_num];
	gettimeofday(&start,0);
	//create thread
	for(int i=0;i<thread_num;i++){
		pthread_create(&p[i],NULL,smooth,cal_range+i*2);
	}
 	for(int i=0;i<thread_num;i++){
		pthread_join(p[i],NULL);
	}
	//thread end
	gettimeofday(&end,0);
	//close semaphore
	sem_destroy(&count_sem);
	for(int i=0;i<2;i++){
		sem_destroy(&barrier_sem[i]);
	}
	for(int i=0;i<bmpInfo.biHeight;i++){
		for(int j=0;j<bmpInfo.biWidth;j++){
			BMPSaveData[i][j]=BMPData[i+1][j];
		}
	}
        if ( saveBMP( outfileName ) )
                cout << "Save file successfully!!" << endl;
        else
                cout << "Save file fails!!" << endl;
	long seconds=end.tv_sec-start.tv_sec;
	long usec=end.tv_usec-start.tv_usec;
	double time=seconds+usec*1e-6;
    	cout << "The execution time = "<< time<<endl ;

	free(BMPData[0]);
 	free(BMPSaveData[0]);
 	free(BMPData);
 	free(BMPSaveData);

    return 0;
}
void* smooth(void *nums){
	//arr[0] is the begin of a thread calculation. arr[1] is the end.
	int* arr=(int*)nums;
	for(int count = 0; count < NSmooth ; count ++){
		//fill the buffer
		for(int i=0;i<bmpInfo.biWidth;i++){
			BMPData[0][i]=BMPData[bmpInfo.biHeight][i];
			BMPData[bmpInfo.biHeight+1][i]=BMPData[1][i];
		}

		//use semaphore to build the barrier, and also let the last thread to swap the data
		//and the last one also have to open the lock
		sem_wait(&count_sem);
		if(counter==thread_num-1){
			counter=0;
			swap(individual_data,BMPData);
			sem_post(&count_sem);
			for(int j=0;j<thread_num-1;j++){
				sem_post(&barrier_sem[count%2]);
			}
		}
		else{
			counter++;
			sem_post(&count_sem);
			sem_wait(&barrier_sem[count%2]);
		}
		//I implement this method by using shared data. That's why I dont need to worry about buffer except for head and tail of the data.
		for(int i = arr[0]; i<arr[1] ; i++)
			for(int j =0; j<bmpInfo.biWidth ; j++){
				int Top = i;
				int Down = i+2;
				int Left = j>0 ? j-1 : bmpInfo.biWidth-1;
				int Right = j<bmpInfo.biWidth-1 ? j+1 : 0;
				individual_data[i+1][j].rgbBlue =  (double) (BMPData[i+1][j].rgbBlue+BMPData[Top][j].rgbBlue+BMPData[Top][Left].rgbBlue+BMPData[Top][Right].rgbBlue+BMPData[Down][j].rgbBlue+BMPData[Down][Left].rgbBlue+BMPData[Down][Right].rgbBlue+BMPData[+i+1][Left].rgbBlue+BMPData[i+1][Right].rgbBlue)/9+0.5;
				individual_data[i+1][j].rgbGreen =  (double) (BMPData[i+1][j].rgbGreen+BMPData[Top][j].rgbGreen+BMPData[Top][Left].rgbGreen+BMPData[Top][Right].rgbGreen+BMPData[Down][j].rgbGreen+BMPData[Down][Left].rgbGreen+BMPData[Down][Right].rgbGreen+BMPData[i+1][Left].rgbGreen+BMPData[i+1][Right].rgbGreen)/9+0.5;
				individual_data[i+1][j].rgbRed =  (double) (BMPData[i+1][j].rgbRed+BMPData[Top][j].rgbRed+BMPData[Top][Left].rgbRed+BMPData[Top][Right].rgbRed+BMPData[Down][j].rgbRed+BMPData[Down][Left].rgbRed+BMPData[Down][Right].rgbRed+BMPData[i+1][Left].rgbRed+BMPData[i+1][Right].rgbRed)/9+0.5;
			}
	}
}

/*********************************************************/
/* Ū������                                              */
/*********************************************************/
int readBMP(char *fileName)
{
	//�إ߿�J�ɮת���	
        ifstream bmpFile( fileName, ios::in | ios::binary );
 
        //�ɮ׵L�k�}��
        if ( !bmpFile ){
                cout << "It can't open file!!" << endl;
                return 0;
        }
 
        //Ū��BMP���ɪ����Y���
    	bmpFile.read( ( char* ) &bmpHeader, sizeof( BMPHEADER ) );
 
        //�P�M�O�_��BMP����
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }
 
        //Ū��BMP����T
        bmpFile.read( ( char* ) &bmpInfo, sizeof( BMPINFO ) );
        
        //�P�_�줸�`�׬O�_��24 bits
        if ( bmpInfo.biBitCount != 24 ){
                cout << "The file is not 24 bits!!" << endl;
                return 0;
        }

        //�ץ��Ϥ����e�׬�4������
        while( bmpInfo.biWidth % 4 != 0 )
        	bmpInfo.biWidth++;

        //�ʺA���t�O����
        BMPSaveData = alloc_memory( bmpInfo.biHeight, bmpInfo.biWidth);
        
        //Ū���������
    	//for(int i = 0; i < bmpInfo.biHeight; i++)
        //	bmpFile.read( (char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE));
	    bmpFile.read( (char* )BMPSaveData[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight);
	
        //�����ɮ�
        bmpFile.close();
 
        return 1;
 
}
/*********************************************************/
/* �x�s����                                              */
/*********************************************************/
int saveBMP( char *fileName)
{
 	//�P�M�O�_��BMP����
        if( bmpHeader.bfType != 0x4d42 ){
                cout << "This file is not .BMP!!" << endl ;
                return 0;
        }
        
 	//�إ߿�X�ɮת���
        ofstream newFile( fileName,  ios:: out | ios::binary );
 
        //�ɮ׵L�k�إ�
        if ( !newFile ){
                cout << "The File can't create!!" << endl;
                return 0;
        }
 	
        //�g�JBMP���ɪ����Y���
        newFile.write( ( char* )&bmpHeader, sizeof( BMPHEADER ) );

	//�g�JBMP����T
        newFile.write( ( char* )&bmpInfo, sizeof( BMPINFO ) );

        //�g�J�������
        //for( int i = 0; i < bmpInfo.biHeight; i++ )
        //        newFile.write( ( char* )BMPSaveData[i], bmpInfo.biWidth*sizeof(RGBTRIPLE) );
        newFile.write( ( char* )BMPSaveData[0], bmpInfo.biWidth*sizeof(RGBTRIPLE)*bmpInfo.biHeight );

        //�g�J�ɮ�
        newFile.close();
 
        return 1;
 
}


/*********************************************************/
/* ���t�O����G�^�Ǭ�Y*X���x�}                           */
/*********************************************************/
RGBTRIPLE **alloc_memory(int Y, int X )
{        
	//�إߪ��׬�Y�����а}�C
        RGBTRIPLE **temp = new RGBTRIPLE *[ Y ];
	RGBTRIPLE *temp2 = new RGBTRIPLE [ Y * X ];
        memset( temp, 0, sizeof( RGBTRIPLE ) * Y);
        memset( temp2, 0, sizeof( RGBTRIPLE ) * Y * X );

	//��C�ӫ��а}�C�̪����Ыŧi�@�Ӫ��׬�X���}�C 
        for( int i = 0; i < Y; i++){
                temp[ i ] = &temp2[i*X];
        }
 
        return temp;
 
}
/*********************************************************/
/* �洫�G�ӫ���                                          */
/*********************************************************/
void swap(RGBTRIPLE *a, RGBTRIPLE *b)
{
	RGBTRIPLE *temp;
	temp = a;
	a = b;
	b = temp;
}

