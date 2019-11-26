/**
 ** This is muminus.c
 ** November 2019 -> Modified for the new CC_USB by Michalis Bachtis
 ** 30sep11 modify to write 4 QDC words if and only if #define QDCflag
 ** 25aug11 add #define QDCflag to build read/write code for 7166 QDC
 ** Put in measured pedestals from 5 july
 ** muminus.c 8jul2011. Change polling loop to trigger event readout if
 ** TDC OR QDC records something. Works for either data or pulser trigger
 ** muminus.c 7jul2011. Works OK but QDC uncalibrated. Good restarting version
 ** w.s. muminus.c version 2.0 Add a phil7166 QDC to the mix
 ** w.s. phil7186.c version v2.3 1Feb11. Add debug flag for conditional
 ** monitor view of TDC output. 
 ** This is final version.To find pedestals set zeros in pedset array
 ** set CR=6, i.e., leave out raw data pedestal correction..
 ** new chan 1-8 pedestals from 18jan11 and 9-16 from 31jan11
 ** Use crate LAM test in pooling loop while waiting for the LAM
 ** running test mode with pulser. Logic uses only com to start
 ** Read Phillips 7186D 4ns 16channel TDC 180Fowns this one;
 ** The borrowed Lecroy 4208 goes back to FNAL
 **/


//#include <time.h>
//#include "ieee_fun_types.h"
//#include <stdio.h>
//#include <stdlib.h>

#include <libxxusb.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <cstdlib>
#include <string.h>
#include "TFile.h"
#include "TTree.h"


int main(int argc, char **argv) {

  time_t lt;
  //Define ptrs to the CC_USB devices 
  int ret;
  xxusb_device_type devices[100]; 
  struct usb_device *dev;
  usb_dev_handle *udev;       // Device Handle 

  //pedestals
  short pedset[16]={5,10,14,24, 12,13,18,23, 7,9,16,27, 14,14,20,26};
  unsigned short lower[16];
  unsigned short upper[16];
  for (uint i=0;i<16;i++) {
    lower[i]=3+pedset[i];
    upper[i]=1;
  }
  for(uint i=0;i<8;i++) upper[i]=4094;
  
  
  //Open root stuff
  TFile * f = new TFile("muminus.root","RECREATE");

  int TDC[16];
  ULong64_t treetime;
  ULong64_t NEVENT=0;
  

  TTree * t = new TTree("data","data");

  t->Branch("TDC",TDC,"TDC[16]/I");
  t->Branch("systime",&treetime);
  t->Branch("EVENT",&NEVENT);
  
    
  //Find XX_USB devices and open the first one found
  xxusb_devices_find(devices);
  dev = devices[0].usbdev;
  udev = xxusb_device_open(dev); 
  // Make sure CC_USB opened OK
  if(!udev) 
    {
      printf ("\n\nFailedto Open CC_USB \n\n");
      return 0;
    }

    long CamD;
    int CamQ, CamX;
    FILE *fp;

    CAMAC_Z(udev);
    CAMAC_I(udev,true);
    CAMAC_C(udev);
    uint slot=13;
    long lam_mask = (1<<(slot-1));
    CAMAC_write_LAM_mask(udev,lam_mask);   
    long lam_mask_read_back=0;
    CAMAC_read_LAM_mask(udev,&lam_mask_read_back);
    fprintf(stderr,"Initialization Completed, LAM mask readback-> %#06x\n", lam_mask_read_back);
    CAMAC_I(udev,false);

    //Set the pedestals and thresholds
    
    for (uint ch=0;ch<16;++ch) {
      //select pedestal
      ret = CAMAC_write(udev,slot, 0, 17, ch,&CamQ, &CamX);
      //printf("select pedestal memory result=%d  Q=%d X=%d \n",ret,CamQ,CamX);
      //write pedestal
      ret = CAMAC_write(udev,slot, ch, 20, pedset[ch],&CamQ, &CamX);
      //printf("write pedestal memory result=%d  Q=%d X=%d \n",ret,CamQ,CamX);

      //select lower threshold
      ret = CAMAC_write(udev,slot, 1, 17, ch,&CamQ, &CamX);
      //printf("select lower threshold  memory result=%d  Q=%d X=%d \n",ret,CamQ,CamX);

      //write lower
      ret = CAMAC_write(udev,slot, ch, 20, lower[ch],&CamQ, &CamX);
      //printf("write lower threshold memory result=%d  Q=%d X=%d \n",ret,CamQ,CamX);

      //select higher threshold
      ret = CAMAC_write(udev,slot, 2, 17, ch,&CamQ, &CamX);
      //printf("select higher threshold  memory result=%d  Q=%d X=%d \n",ret,CamQ,CamX);

      //write lower
      ret = CAMAC_write(udev,slot, ch, 20, upper[ch],&CamQ, &CamX);
      //printf("write higher threshold memory result=%d  Q=%d X=%d \n",ret,CamQ,CamX);
      

    }
    //Reset the register
    ret = CAMAC_read(udev,slot, 3, 11, &CamD,&CamQ, &CamX);
    //printf("reset register result=%d  Q=%d X=%d \n",ret,CamQ,CamX);

    //Set control register
    ret = CAMAC_write(udev,slot, 0, 19, 7,&CamQ, &CamX);
    //printf("set control register result=%d  Q=%d X=%d \n",ret,CamQ,CamX);

    //redback control register
    ret = CAMAC_read(udev,slot, 0, 6, &CamD,&CamQ, &CamX);
    //printf("readback control register result=%d  D=%d Q=%d X=%d \n",ret,CamD,CamQ,CamX);

    //enable lam
    ret = CAMAC_read(udev,slot, 0, 26, &CamD,&CamQ, &CamX);
    //printf("enable LAM result=%d  D=%d Q=%d X=%d \n",ret,CamD,CamQ,CamX);

    //read hit register
    //    ret = CAMAC_read(udev,slot, 1, 6, &CamD,&CamQ, &CamX);
    //printf("read hit register result=%d  D=%d Q=%d X=%d \n",ret,CamD,CamQ,CamX);
    
    
    
    
    fprintf(stderr,"Ready to start taking data\n");   
    while(1) {
        if ((fp = fopen("stop", "r")) != NULL) {
	  fprintf(stderr,"stop present, closing device and exiting \n");
	  f->Write();
	  f->Close();
	  xxusb_device_close(udev);
	  exit(0);
	}

	for (uint i=0;i<16;++i)
	  TDC[i]=-999;
	

	//Test LAM for modules
	ret = CAMAC_read(udev, 25, 10, 0, &CamD,&CamQ, &CamX);
	bool moduleFired = (CamD & (1<<(slot-1)) )!=0;

	//Read module LAM
	ret = CAMAC_read(udev,slot, 0, 8, &CamD,&CamQ, &CamX);
	//	if (CamQ)
	//	  printf("read module LAM result=%d  D=%d Q=%d X=%d \n",ret,CamD,CamQ,CamX);
	moduleFired=moduleFired&&CamQ;

	  
	//Read TDC Values	
	if (moduleFired) {
	  //Read hit register
	  ret = CAMAC_read(udev,slot, 1, 6, &CamD,&CamQ, &CamX);
	  uint hitMap=CamD;
	  if (hitMap==0)
	    continue;
	  for (uint i=0;i<16;++i) {
	    if (((1<<i) & hitMap)==0)
	      continue;
	    //Read data register
	    ret = CAMAC_read(udev,slot, i, 0, &CamD,&CamQ, &CamX);
	    //   printf("read data result=%d  D=%d Q=%d X=%d \n",ret,CamD,CamQ,CamX);
	    TDC[i]=CamD &0xfff;
	  }

	  lt=time(NULL);
	  treetime=ULong64_t(lt);
	  t->Fill();
	  NEVENT++;
	  printf ("%d %d ",NEVENT,treetime);
	  for (uint i=0;i<16;++i)
	    printf("%d ",TDC[i]);
	  printf("\n");

	  ret = CAMAC_read(udev,slot, 3, 11, &CamD,&CamQ, &CamX);
	  //printf("reset result=%d  D=%d Q=%d X=%d \n",ret,CamD,CamQ,CamX);
	  
	}
    }
    
}


