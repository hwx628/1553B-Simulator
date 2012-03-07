// Sim1553B_chan.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <string.h>

#include "simbc.h"
#include "simmt.h"
#include "simrt.h"
#include "wordParser.h"

typedef struct busDataStructure
{
	UINT16 type;
	UINT16 data[32];
	UINT16 time;
	UINT16 full;
	UINT16 len;
} TMPBUSStructure;


TMPBUSStructure buffData;

bool hasRead = false;
bool hasData = false;

SimBC *bc = NULL;
SimRT *rt = NULL;
SimMT *mt = NULL;
UINT16 RTBCMTMode = 4;//Invalid mode
UINT16 rt_address = 33;//NOTE this is not a valid address

char * cStrTrim(char *&str, int len)
{
	if (!str)
	{
		return NULL;
	}
	if (!len)
	{
		len = strlen(str);
	}
	int lIndex = 0;
	while(lIndex < len)
	{
		if(str[lIndex] == ' ' || str[lIndex] == '\t' || str[lIndex] == '\r' || str[lIndex] == '\n')
		{
			lIndex ++; 
		}
		else
		{
			break;
		}
	}
	if (lIndex == len)
	{
		return NULL;
	}
	
	str += lIndex;
	return str;
}


UINT16 loadConfiguration(const char * filePath,bool isInternalTest)
{
	FILE *configureFile = fopen(filePath,"r");
	if (!configureFile)
	{
		return 1;
	}
	char dataBuff[102400] = {0};
	char *dataPtr = dataBuff;
	fread(dataBuff,102400,sizeof(char),configureFile);
	if (!feof(configureFile))//Buff is insufficient
	{
		fclose(configureFile);
		return 1;
	}
	char *pdef;
	//Current First Line;
	pdef = strstr(dataPtr,"\n");
	*pdef = '\0';
	pdef ++;
	
	
	if(dataBuff) 
	{	
		if(dataBuff[0] == 'R' && dataPtr[1] == 'T')
		{
			RTBCMTMode = 1;
			int rtAddress = 33;
			sscanf(dataPtr + 2,"%x",&rtAddress);
			if(rtAddress && rtAddress < 32)
			{
				rt = new SimRT(rtAddress);
			}
			else
			{
				//RT address is not valid
				fclose(configureFile);
				return 1;
			}
			llogDebug("Init","RT Mode, and address: 0x%x",rtAddress);
		}
		else if(dataPtr[0] == 'B' && dataPtr[1] == 'C')
		{
			RTBCMTMode = 0;
			bc = new SimBC;
			llogDebug("Init","BC Mode");
		}
		else if(dataPtr[0] == 'B' && dataPtr[1] == 'M')
		{
			RTBCMTMode = 2;
			mt = new SimMT;
			llogDebug("Init","MT Mode");
		}
		else
		{
			//Error
			fclose(configureFile);
			return 1;
		}
		dataPtr = pdef;
		pdef = strstr(dataPtr,"\n");
		if(pdef)
		{
			*pdef = '\0';
			pdef ++;
		}
		
	}
	
	if (isInternalTest)
	{
		while (dataPtr && *dataPtr != '\0')
		{
			char mode = '\0';
			int address = 0;
			int data = 0;
			sscanf(dataPtr,"%c%x%x",&mode,&address,&data);
			if(mode == 'R')
			{
				if(RTBCMTMode == 0 && bc)//BC
				{
					bc->regWriteToAddr(address,data);
				}
				else if(RTBCMTMode == 1 && rt)//RT
				{
					rt->regWriteToAddr(address,data);
				}
				else if(RTBCMTMode == 2 && mt)//MT
				{
					mt->regWriteToAddr(address,data);
				}
				llogDebug("Init","Reg 0x%x : 0x%2x",address,data);
			}
			else if(mode == 'M')
			{
				if(RTBCMTMode == 0 && bc)//BC
				{
					bc->memWrite(address,data);
				}
				else if(RTBCMTMode == 1 && rt)//RT
				{
					rt->memWrite(address,data);
				}
				else if(RTBCMTMode == 2 && mt)//MT
				{
					mt->memWrite(address,data);
				}
				llogDebug("Init","Mem 0x%x : 0x%2x",address,data);
			}

			dataPtr = pdef;
			if(!dataPtr)
			{
				break;
			}
			pdef = strstr(dataPtr,"\n");
			if(pdef)
			{
				*pdef = '\0';
				pdef ++;
			}
		} 
	}
	
	
	fclose(configureFile);
	return 0;
}





UINT32 internalCheckRecv(UINT32 len,void *recvData)
{

	if( len >= 4 * sizeof(UINT16))
	{
		//UINT16 *buffPtr = NULL;
		if(hasData)
		{
			//buffPtr = buffData;
		}
		else 
		{
			return 1;
		}
		hasRead = true;
		(*((UINT16*)recvData + 0)) = buffData.type;
		(*((UINT16*)recvData + 1)) = buffData.data[0];
		(*((UINT16*)recvData + 2)) = buffData.time;
		(*((UINT16*)recvData + 3)) = buffData.full;
		return 0;
	}
	return 1;
	
}
extern "C"  void Init(int argc,const char *argv[],pfun_RecvCheck fromSyn,sim61580irq irq)
{

	GenIRQ = irq;
	CheckRecv = fromSyn;
	loadConfiguration("configure.txt",false);

}
extern "C"  void Step(void)
{
	if(RTBCMTMode == 0 && bc)//BC
	{
		bc->bcStep();
	}
	else if(RTBCMTMode == 1 && rt)//RT
	{
		rt->RTStep();
	}
	else if(RTBCMTMode == 2 && mt)//MT
	{
		mt->mtStep();
	}
}
extern "C"  void Exit(void)
{
	if(RTBCMTMode == 0 && bc)//BC
	{
		delete bc;
		bc = NULL;
	}
	else if(RTBCMTMode == 1 && rt)//RT
	{
		delete rt;
		rt = NULL;
	}
	else if(RTBCMTMode == 2 && mt)//MT
	{
		delete mt;
		mt = NULL;
	}
	
}
extern "C"  UINT32 Read(UINT64 timestamp, UINT32 Addr, void *data)
{
	UINT16 realAddr;	

	if(RTBCMTMode == 0 && bc)//BC
	{
		if (Addr&0XF000)//mem
		{
			realAddr=Addr&0XFFF;
			*((UINT16*)data) = bc->memRead(realAddr);
		}
		else
		{
			realAddr=Addr&0XF;
			*((UINT16*)data) = bc->regReadFromAddr(realAddr);
		}

	}
	else if(RTBCMTMode == 1 && rt)//RT
	{
		if (Addr&0XF000)//mem
		{
			realAddr=Addr&0XFFF;
			*((UINT16*)data) = rt->memRead(realAddr);
		}
		else
		{
			realAddr=Addr&0XF;
			*((UINT16*)data) = rt->regReadFromAddr(realAddr);
		}
	}
	else if(RTBCMTMode == 2 && mt)//MT
	{
		if (Addr&0XF000)//mem
		{
			realAddr=Addr&0XFFF;
			*((UINT16*)data) = mt->memRead(realAddr);
		}
		else
		{
			realAddr=Addr&0XF;
			*((UINT16*)data) = mt->regReadFromAddr(realAddr);
		}
	}
	else
	{
		return 0;
	}
	
	return *((UINT16*)data);
}
extern "C"  UINT32 Write(UINT64 timestamp, UINT32 Addr, void *data)
{
	UINT16 realAddr;	
	UINT16 dataU16 = *((UINT16*)data);
	if(RTBCMTMode == 0 && bc)//BC
	{

		if (Addr&0XF000)//mem
		{
			realAddr=Addr&0XFFF;
			llogInfo("Write","Mem 0x%x:0x%x",realAddr,dataU16);
			return bc->memWrite(realAddr,dataU16);
		}
		else
		{
			realAddr=Addr&0XF;
			llogInfo("Write","Reg 0x%x:0x%x",realAddr,dataU16);
			return bc->regWriteToAddr(realAddr,dataU16);
		}

		
	}
	else if(RTBCMTMode == 1 && rt)//RT
	{
		if (Addr&0XF000)//mem
		{
			realAddr=Addr&0XFFF;
			return rt->memWrite(realAddr,dataU16);
		}
		else
		{
			realAddr=Addr&0XF;
			return rt->regWriteToAddr(realAddr,dataU16);
		}
	}
	else if(RTBCMTMode == 2 && mt)//MT
	{
		if (Addr&0XF000)//mem
		{
			realAddr=Addr&0XFFF;
			return mt->memWrite(realAddr,dataU16);
		}
		else
		{
			realAddr=Addr&0XF;
			return mt->regWriteToAddr(realAddr,dataU16);
		}
	}
	return 0;
}
extern "C"  UINT32 OnData(UINT32 len, void *data)
{
	if(RTBCMTMode == 0 && bc)//BC
	{
		return bc->OnData(len,data);
	}
	else if(RTBCMTMode == 1 && rt)//RT
	{
		return rt->OnData(len,data);
	}
	else if(RTBCMTMode == 2 && mt)//MT
	{
		return mt->OnData(len,data);
	}
	return 0;
}

extern "C"  void SaveState()
{
	return; 
}

extern "C"  void RestoreState()
{
	return; 
}


int _tmain(int argc, _TCHAR* argv[])
{	
	llogDebug("123","message %d",123);
	
	char *testStr = "R      0x1234    0x123456       ";
	char letter = '\0';
	int address = 0;
	int data = 0;
	UINT16 buff[4] = {0};
	sscanf(testStr,"%c%x%x",&letter,&address,&data);
	//loadConfiguration("rtconfigure.txt");
	Read(0,0xffffff,buff);
	Write(0,0xffffff,buff);
	
	CheckRecv = internalCheckRecv;
	//Load BC configuration
	SimBC internalBC("bcconfigure.txt");
	//Load RT configuration for RT address 1
	SimRT internalRT1(1,"rtconfigure1.txt");
	//Load RT configuration for RT address 2
	SimRT internalRT2(2,"rtconfigure2.txt");
	//Load RT configuration for RT address 3
	SimRT internalRT3(3,"rtconfigure3.txt");
	//Load RT configuration for RT address 4
	SimRT internalRT4(4,"rtconfigure4.txt");
	SimMT internalMT("mtconfigure.txt");
	while(1)
	{
		internalBC.bcStep();
		internalRT1.RTStep();
		internalRT2.RTStep();
		internalRT3.RTStep();
		internalRT4.RTStep();
		internalMT.mtStep();
		hasData = false;
		if(1 == internalBC.OnData(sizeof(TMPBUSStructure),&buffData)		|| 
				1 == internalRT1.OnData(sizeof(TMPBUSStructure),&buffData)  || 
				1 == internalRT2.OnData(sizeof(TMPBUSStructure),&buffData)  || 
				1 == internalRT3.OnData(sizeof(TMPBUSStructure),&buffData)  ||  
				1 == internalRT4.OnData(sizeof(TMPBUSStructure),&buffData) ||
				1 == internalMT.OnData(sizeof(TMPBUSStructure),&buffData))
		{
			hasData = true;
			
		}

	}
	return 0;
}

