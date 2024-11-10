/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : simple_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */
#pragma warning(disable: 4013)
#pragma warning(disable: 4819)
#include <stdio.h>
#include <string.h>
//#include <Mmsystem.h>

#include "osal.h"
#include "ethercat.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
volatile int rtcnt;
boolean inOP;
uint8 currentgroup = 0;

#include <windows.h> 
#include <time.h>  // Include this header for CLOCK_REALTIME


void setThreadAffinity(OSAL_THREAD_HANDLE thread, int cpuCore) {
    DWORD_PTR mask = 1 << cpuCore; // Create a mask for the specified CPU core
    SetThreadAffinityMask(thread, mask);
}



/* most basic RT thread for process data, just does IO transfer */
void CALLBACK RTthread(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1,  DWORD_PTR dw2)
{


    DWORD_PTR mask = 1 << 1; // CPU2 is core 1 (0-based index)
    SetThreadAffinityMask(GetCurrentThread(), mask);

    IOmap[0]++;
    ec_send_processdata();
    wkc = ec_receive_processdata(EC_TIMEOUTRET);
    rtcnt++;
    /* do RT control stuff here */
}

int eRobsetup(uint16 slave)
{
    int retval;
    uint16 u16val;
    //uint32 u32val;
    // map velocity
    //0x6064 32 bit position actual value
    //0x6041 16 bit status word
    //0x606c 16 bit velocity actual value
    uint16 map_1c12[2] = {0x0001,  0x1606};

    //0x607A 32 bit Target position
    //0x6040 16 bit Control word
    //0x60ff 32 bit Target velocity
    uint16 map_1c13[2] = {0x0001, 0x1a03};

    retval = 0;

    // Set PDO mapping using Complete Access
    // Strange, writing CA works, reading CA doesn't
    // This is a protocol error of the slave.
    retval += ec_SDOwrite(slave, 0x1c12, 0x00, TRUE, sizeof(map_1c12), &map_1c12, EC_TIMEOUTSAFE);
    retval += ec_SDOwrite(slave, 0x1c13, 0x00, TRUE, sizeof(map_1c13), &map_1c13, EC_TIMEOUTSAFE);

    // bug in EL7031 old firmware, CompleteAccess for reading is not supported even if the slave says it is.
    ec_slave[slave].CoEdetails &= ~ECT_COEDET_SDOCA;

    // set some motor parameters, just as example
      u16val = 1200; // max motor current in mA
    //retval += ec_SDOwrite(slave, 0x8010, 0x01, FALSE, sizeof(u16val), &u16val, EC_TIMEOUTSAFE);
    //u16val = 150; // motor coil resistance in 0.01ohm
    //set other nescessary parameters as needed
    // .....

    while(EcatError) printf("%s", ec_elist2string());

    printf("eRob slave %d set, retval = %d\n", slave, retval);
    return 1;
}


int sCount = 0;
int test_number = 1;
void simpletest(char *ifname)
{
    
   int k = 0;

   while ( k< test_number)
{

      k++;

    int i, j, oloop, iloop, wkc_count, chk, slc;
    UINT mmResult;

    needlf = FALSE;
    inOP = FALSE;
    
    int retval = 0;
    uint16 u16val = 0 ;

   printf("Starting simple test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ec_init(ifname))
   {
      printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */


       if ( ec_config_init(FALSE) > 0 )
       {
         printf("%d slaves found and configured.\n",ec_slavecount);
         
         for (int i = 1; i <= ec_slavecount; i++) {
            // (void)ecx_FPWR(ecx_context.port, i, ECT_REG_DCSYNCACT, sizeof(WA), &WA, 5 * EC_TIMEOUTRET);
            printf("Name: %s\n", ec_slave[i].name);
            printf("Slave %d: Type %d, Address 0x%02x, State Machine actual %d, required %d\n", i, ec_slave[i].eep_id, ec_slave[i].configadr, ec_slave[i].state, EC_STATE_INIT);
            printf("___________________________________________\n");
           // ecx_dcsync0(&ecx_context, i, TRUE, 5000000, 0);
         }

         if((ec_slavecount >= 1))
         {
             for(slc = 1; slc <= ec_slavecount; slc++)
             {
                  //ec_dcsync0(slc,FALSE,1000000,0);
                //printf("ec_slave[slc].eep_man: %d",ec_slave[slc].eep_man);
                 // beckhoff EL7031, using ec_slave[].name is not very reliable
                 if((ec_slave[slc].eep_man == 0x5a65726f) && (ec_slave[slc].eep_id == 0x00029252))
                 {

                     
                     printf("Found %s at position %d\n", ec_slave[slc].name, slc);
                     // link slave specific setup to preop->safeop hook
                     ec_slave[slc].PO2SOconfig = &eRobsetup;
                 }
             }
         }

         ec_statecheck(0, EC_STATE_PRE_OP,  EC_TIMEOUTSTATE * 5);

   

          for(slc = 1; slc <= ec_slavecount; slc++)
             {
                ec_dcsync0(slc,TRUE,1000000,0);   // 82%
               
             }
             ec_configdc();   // 9%


         ec_config_map(&IOmap);  // 

         //ec_configdc();
      
         printf("Slaves mapped, state to SAFE_OP.\n");
         /* wait for all slaves to reach SAFE_OP state */
         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);


         //ec_dcsync0(1,TRUE,1000000,0);
         //ec_configdc();

         oloop = ec_slave[0].Obytes;
         if ((oloop == 0) && (ec_slave[0].Obits > 0)) oloop = 1;
         if (oloop > 12) oloop = 12;
         iloop = ec_slave[0].Ibytes;
         if ((iloop == 0) && (ec_slave[0].Ibits > 0)) iloop = 1;
         if (iloop > 12) iloop = 12;


         uint16 dc_syn ;
         int rdl = sizeof(dc_syn);
         ec_SDOread(1,0x1C32,0x01,FALSE,&rdl,&dc_syn,EC_TIMEOUT);
         printf("DC synchronized 0x1c32: %d\n",dc_syn);
         printf("segments : %d : %d %d %d %d\n",ec_group[0].nsegments ,ec_group[0].IOsegment[0],ec_group[0].IOsegment[1],ec_group[0].IOsegment[2],ec_group[0].IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         ec_send_processdata();
         ec_receive_processdata(EC_TIMEOUTRET);

         /* start RT thread as periodic MM timer */
         mmResult = timeSetEvent(1, 0, RTthread, 0, TIME_PERIODIC);

         /* request OP state for all slaves */
         ec_writestate(0);
         chk = 200;
         /* wait for all slaves to reach OP state */
         do
         {
            ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
         if (ec_slave[0].state == EC_STATE_OPERATIONAL )
         {
            printf("Operational state reached for all slaves.\n");
            wkc_count = 0;
            inOP = TRUE;
            sCount ++;

            int step = 0;
            uint8 u8val = 9;

          for(slc = 1; slc <= ec_slavecount; slc++)
             {
               retval += ec_SDOwrite(slc, 0x6060, 0x00, FALSE, sizeof(u8val), &u8val, 5*EC_TIMEOUTSAFE);
             }


            uint16 control_word,state_word;
             rdl = sizeof(control_word);

         //ec_SDOread(1,0x6040,0x00,FALSE,&rdl,&control_word,EC_TIMEOUT);

            ec_SDOread(1,0x6040,0x00,FALSE,&rdl,&control_word,EC_TIMEOUT);
            ec_SDOread(1,0x6041,0x00,FALSE,&rdl,&state_word,EC_TIMEOUT);

           // uint16 dc_syn ;
            rdl = sizeof(dc_syn);
            ec_SDOread(1,0x1C32,0x01,FALSE,&rdl,&dc_syn,EC_TIMEOUT);
            printf("control_word: %d\n",control_word);
            printf("state_word: %d\n",state_word);
            printf("DC synchronized 0x1c32: %d\n",dc_syn);

            int64 sync0_time = 0;
            int64 sm2_time = 0;
            /* cyclic loop, reads data from RT thread */
            while(rtcnt<3*1000*60)
            {
                    if(wkc >= expectedWKC)
                    {
                        printf("Processdata cycle %4d, WKC %d , O:", rtcnt, wkc);

                        for(j = 0 ; j < oloop; j++)
                        {
                            printf(" %2.2x", *(ec_slave[0].outputs + j));
                        }
                    
                        printf(" I:");
                        for(j = 0 ; j < iloop; j++)
                        {
                            printf(" %2.2x", *(ec_slave[0].inputs + j));
                        }
                        printf(" T:%lld\r",ec_DCtime);
                        fflush(stdout);
                        needlf = TRUE;
                    }
                 // 
                uint32_t target_position = 0; // 
                uint32_t target_velocity = 0; // 
                uint16 output_data[10] = {0};
            if (step > 6)
            {
                
                output_data[0] = 0x00;          // 
                output_data[1] = 0x00; // 
                output_data[2] = 0x00;          //  
                output_data[3] = 0x00; // 
                output_data[4] = target_velocity & 0xFFFF;          // 
                output_data[5] = (target_velocity >> 16) & 0xFFFF; // 
                output_data[6] = 0x00; // 
                output_data[7] = 0x00; // 
                output_data[8] = 0x00; //
                output_data[9] = 0x0f; //  
                            //step = 1;
            }
            if (step <= 6 && step > 4)
            {
                
                output_data[0] = 0x00;          // 
                output_data[1] = 0x00; // 
                output_data[2] = 0x00;          //  
                output_data[3] = 0x00; // 
                output_data[4] = target_velocity & 0xFFFF;          // 
                output_data[5] = (target_velocity >> 16) & 0xFFFF; // 
                output_data[6] = 0x00; // 
                output_data[7] = 0x00; // 
                output_data[8] = 0x00; //
                output_data[9] = 0x07; //  
               // memcpy(ec_slave[0].outputs, output_data, sizeof(output_data)); //
                //step = 3;

            }
            if (step <= 4 && step >2 )
            {
               
                output_data[0] = 0x00;          // 
                output_data[1] = 0x00; // 
                output_data[2] = 0x00;          //  
                output_data[3] = 0x00; // 
                output_data[4] = 0x00;          // 
                output_data[5] = 0x00; // 
                output_data[6] = 0x00; // 
                output_data[7] = 0x00; // 
                output_data[8] = 0x00; //
                output_data[9] = 0x06; //  
               // memcpy(ec_slave[0].outputs, output_data, sizeof(output_data)); //
                //step = 2;
            }
            if (step <= 2)
            {
                output_data[0] = 0x00;          // 
                output_data[1] = 0x00; // 
                output_data[2] = 0x00;          //  
                output_data[3] = 0x00; // 
                output_data[4] = 0x00;          // 
                output_data[5] = 0x00; // 
                output_data[6] = 0x00; // 
                output_data[7] = 0x00; // 
                output_data[8] = 0x00; //
                output_data[9] = 0xf0; //  
                //memcpy(ec_slave[0].outputs, output_data, sizeof(output_data)); //  
            }
               
               memcpy(ec_slave[0].outputs, output_data, sizeof(output_data)); //

            /*
            for (int i = 1; i < ec_slavecount; i++)
            {
            memcpy(ec_slave[i].outputs, output_data, sizeof(output_data)); //
            }
            */
            step += 1;/* code */
            //ec_send_processdata();
            //wkc = ec_receive_processdata(EC_TIMEOUTRET);
            osal_usleep(1000000);
            }
            inOP = FALSE;
         }
         else
         {
                printf("Not all slaves reached operational state.\n");
                ec_readstate();
                for(i = 1; i<=ec_slavecount ; i++)
                {
                    if(ec_slave[i].state != EC_STATE_OPERATIONAL)
                    {
                        printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                            i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                    }
                }
         }

         /* stop RT thread */
         timeKillEvent(mmResult);

         printf("\nRequest init state for all slaves\n");
         ec_slave[0].state = EC_STATE_INIT;
         /* request INIT state for all slaves */
         ec_writestate(0);
        }
        else
        {
            printf("No slaves found!\n");
        }
        printf("End simple test, close socket\n");
        /* stop SOEM, close socket */
        ec_close();
    }
    else
    {
        printf("No socket connection on %s\nExcecute as root\n",ifname);
    }

    osal_usleep(1000000);
   }

   printf("Successful Count: %d", sCount);
}

//DWORD WINAPI ecatcheck( LPVOID lpParam )
OSAL_THREAD_FUNC ecatcheck(void *lpParam)
{
    int slave;

    while(1)
    {
        if( inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
               needlf = FALSE;
               printf("\n");
            }
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++)
            {
               if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
               {
                  ec_group[currentgroup].docheckstate = TRUE;
                  if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
                  {
                     printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                     ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state == EC_STATE_SAFE_OP)
                  {
                     printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                     ec_slave[slave].state = EC_STATE_OPERATIONAL;
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state > EC_STATE_NONE)
                  {
                     if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d reconfigured\n",slave);
                     }
                  }
                  else if(!ec_slave[slave].islost)
                  {
                     /* re-check state */
                     ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                     if (ec_slave[slave].state == EC_STATE_NONE)
                     {
                        ec_slave[slave].islost = TRUE;
                        printf("ERROR : slave %d lost\n",slave);
                     }
                  }
               }
               if (ec_slave[slave].islost)
               {
                  if(ec_slave[slave].state == EC_STATE_NONE)
                  {
                     if (ec_recover_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d recovered\n",slave);
                     }
                  }
                  else
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d found\n",slave);
                  }
               }
            }
            if(!ec_group[currentgroup].docheckstate)
               printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(1000000);
    }

    //return 0;
}

char ifbuf[1024];

int main(int argc, char *argv[])
{
   ec_adaptert * adapter = NULL;
   printf("SOEM (Simple Open EtherCAT Master)\nSimple test\n");

   if (argc > 1)
   {
      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, NULL);
      strcpy(ifbuf, argv[1]);
        // Set the thread affinity to CPU3 (core 3)
        DWORD_PTR mask = 1 << 4; // CPU3 is core 3 (0-based index)
        SetThreadAffinityMask(thread1, mask);
      /* start cyclic part */
      simpletest(ifbuf);
   }
   else
   {
      printf("Usage: simple_test ifname1\n");
   	/* Print the list */
      printf ("Available adapters\n");
      adapter = ec_find_adapters ();
      while (adapter != NULL)
      {
         printf ("Description : %s, Device to use for wpcap: %s\n", adapter->desc,adapter->name);
         adapter = adapter->next;
      }
   }

   printf("End program\n");
   return (0);
}
