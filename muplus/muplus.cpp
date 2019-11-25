/** muplus.c v4.0 . Adapted to the new CC-USB Module (Michalis) 
 ** Rearrange the DAq sequence
 ** muplus.c v3.1 production version. Starts from v3.03
 ** Clear LAM's and enable LAM's globally via controller
 ** Suppress diagnostic prints
 ** Get LAM info exclusively from the crate LAM.
 ** Eliminate the cssa(8,,,(10,,,(26,,, calls to the TDC's
 ** Use a 24 bit read to get the crate
 ** LAM so as to have module LAMs from all 24 slots 
 ** Write run-header and -footer output records to stderr
 ** The modules (J. Kubic's single channel tdcs) in slots 6,7.
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


  //Open root stuff
  TFile * f = new TFile("muplus.root","RECREATE");

  int TDC6,TDC7;
  bool tdcFired6,tdcFired7;
  ULong64_t treetime;
  ULong64_t NEVENT=0;
  

  TTree * t = new TTree("data","data");

  t->Branch("TDC6",&TDC6);
  t->Branch("TDC7",&TDC7);
  t->Branch("TDC6Fired",&tdcFired6);
  t->Branch("TDC7Fired",&tdcFired7);
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
    long lam_mask = (1<<5) | (1<<6);
    CAMAC_write_LAM_mask(udev,lam_mask);   
    long lam_mask_read_back=0;
    CAMAC_read_LAM_mask(udev,&lam_mask_read_back);
    fprintf(stderr,"Initialization Completed, LAM mask readback-> %#06x\n", lam_mask_read_back);
    CAMAC_I(udev,false);

    fprintf(stderr,"Ready to start taking data\n");   
    while(1) {
        if ((fp = fopen("stop", "r")) != NULL) {
	  fprintf(stderr,"stop present, closing device and exiting \n");
	  f->Write();
	  f->Close();
	  xxusb_device_close(udev);
	  exit(0);
	}





	//	ret = CAMAC_read(udev, 6, 0, 10, &CamD,&CamQ, &CamX);
	//	printf("result=%d  D=%d , Q=%d X=%d\n",ret,CamD,CamQ,CamX);
	//ret = CAMAC_read(udev, 7, 0, 10, &CamD,&CamQ, &CamX);
	//printf("result=%d  D=%d , Q=%d X=%d\n",ret,CamD,CamQ,CamX);
	//ret = CAMAC_read(udev, 6, 0, 26, &CamD,&CamQ, &CamX);
	//	printf("result=%d  D=%d , Q=%d X=%d\n",ret,CamD,CamQ,CamX);
	//ret = CAMAC_read(udev, 7, 0, 26, &CamD,&CamQ, &CamX);
	//	printf("result=%d  D=%d , Q=%d X=%d\n",ret,CamD,CamQ,CamX);


	//Start of loop
	tdcFired6=false;
	tdcFired7=false;
	TDC6=-999;
	TDC7=-999;
	//Test LAM for modules
	ret = CAMAC_read(udev, 25, 10, 0, &CamD,&CamQ, &CamX);
	tdcFired6 = (CamD & (1<<5) )!=0;
	tdcFired7 = (CamD & (1<<6) )!=0;
	
	//Read TDC Values	
	if (tdcFired6) {
	  ret = CAMAC_read(udev, 6, 0, 0, &CamD,&CamQ, &CamX);
	  //	  printf("result=%d RAW TDC6 = %d Q=%d X=%d \n",ret,CamD,CamQ,CamX);
	  CamD=CamD & 0xffff;
	  
	    //Overflow check
	  if (CamD<32770)
	    TDC6 = CamD;
	  else 
	    TDC6=-999;
	  ret = CAMAC_read(udev, 6, 0, 10, &CamD,&CamQ, &CamX);
	}
	else {
	  TDC6 = -999;
	}

	if (tdcFired7) {
	  ret = CAMAC_read(udev, 7, 0, 0, &CamD,&CamQ, &CamX);
	  //	  printf("result=%d RAW TDC7 = %d Q=%d X=%d \n",ret,CamD,CamQ,CamX);
	  CamD=CamD & 0xffff;

	  //Overflow check
	  if (CamD<32770)
	    TDC7 = CamD;
	  else
	    TDC7=-999;
	  ret = CAMAC_read(udev, 7, 0, 10, &CamD,&CamQ, &CamX);  
	}
	else {
	  TDC7 = -999;
	}
	

        //Now write an event if at least one of the TDCs fired
	//and clear and enable LAM
	if (TDC6!=-999||TDC7!=-999) {
	  lt=time(NULL);
	  treetime=ULong64_t(lt);
	  printf(" %d %li  %d %d\n",NEVENT,lt,TDC6,TDC7);
	  t->Fill();
	  NEVENT++;
	}



	

    }       /* while 1 */
}


