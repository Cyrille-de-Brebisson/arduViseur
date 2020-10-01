#define PinScr_CS   0  // AD_B0_02
#define PinScr_DC   1  // AD_B0_03
#define PinCamVsync 2  // EMC_04
#define PinCamHref  3  // EMC_05
#define PinCamPpclk 4  // EMC_06
#define PinCamXclk  5  // EMC_08
#define TouchClk    6  // B0_10
#define TouchCS     7  // B1_01
#define TouchDOUT   8  // B1_00
#define TouchDIN    9  // B0_11S
#define PinScr_Led  10 // B0_00
#define PintSrcDOut 11 // B0_02
//#define TouchDIN    12 // B0_01
#define PinScrClk   13 // B0_03
#define PinCamD0    14 // AD_B1_02
#define PinCamD1    15 // AD_B1_03
#define PinCamD3    16 // AD_B1_07
#define PinCamD2    17 // AD_B1_06
#define PinCamI2CCL 18 // AD_B1_01
#define PinCamI2CDA 19 // AD_B1_00
#define PinCamD6    20 // AD_B1_10
#define PinCamD7    21 // AD_B1_11
#define PinCamD4    22 // AD_B1_08
#define PinCamD5    23 // AD_B1_09

///////////////////////////////////////////////////
// HW Interfaces and drivers
#include <EEPROM.h>
#include <ILI9341_t3n.h>
ILI9341_t3n tft = ILI9341_t3n(PinScr_CS, PinScr_DC, 255);

#include "touch.h"
XPT2046_Touchscreen touch(TouchCS, TouchClk, TouchDIN, TouchDOUT);

#include "camera.h"

///////////////////////////////////////////////////
// Hal layer
#include "stardisp.h"
TPixel *getFB(int32_t &w, int32_t &h) // returns framebuffer location and size
{
  w= 320; h= 240; return tft.getFrameBuffer();
}
  static int32_t backLight;
int32_t setBackLight(int32_t v)                    // v from 0 to 100. if <0, returns current back light
{
  if (v<0) return backLight;
  backLight= v;
  pinMode(PinScr_Led, OUTPUT); analogWrite(PinScr_Led, v*255/100);
  return backLight;
}
bool getCamera(uint8_t *&buf, int32_t &w, int32_t &h)    // reads the camera data and get the size. if buf==nullptr, will allocate a new one!
{
  w= cameraWidth(); h= cameraHeight(); buf= cameraImage;
  return cameraRead();
}
bool readTP(int32_t &x, int32_t &y)     // return false if up, else returns true and x/y
{
  return touch.readData(x, y); // if up then just return, if down, update x/y and return true
}
void debugOut(char const *t) { Serial.println(t); delay(200); }
uint32_t getNow() { return millis(); }
uint32_t getButtons() { return 0; }
void saveData(uint8_t *d, int size)
{
  uint8_t crc= 0; for (int i=0; i<size; i++) { crc+= *d; EEPROM[i]= *d++; }
  EEPROM[size]= crc;
}
bool loadData(uint8_t *d, int size)
{
  uint8_t crc= 0; for (int i=0; i<size; i++) { *d= EEPROM[i]; crc+= *d++; }
  return EEPROM[size]==crc;
}

  // date/time functions extracted from napier/timecalc.cpp
  static unsigned int const halNbDaysSince[] =   {  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, };
  static bool halDateLeap(uint32_t y) //return true if y is leap year
  {
    if (y%400==0) return true;
    if (y%100==0) return false;
    return ((y&3)==0);
  }
  static int32_t halDateDayIndexFromDateinternal(uint32_t d, uint32_t m, uint32_t y, bool leap)
  { 
    y=y-1582;
    uint32_t r= y*365;
    r+=(y+2)/4; // every 4th year is leap
    r-=(y+82)/100; // but every 100 is not
    r+=(y+382)/400; // but every 400 is!
    r+= halNbDaysSince[m-1]; // add months
    if (leap && (m<=2)) r--; // adjust for leaps
    r+= d-1; // add current day, 0 based, not 1...
    if (r<273+14) return -1; // if before 15 october 1582, not a good date
    return r;
  }
  static bool halDateFromDayIndex2(uint32_t a, uint32_t *Y, uint32_t *M, uint32_t *D)
  { 
    uint32_t y, m;
    if (a<273+14) return false; // begining of time
    if (a>3074610) return false; // end of time 31 dec 9999
    y= 1582;
    if (a>=365) { y++; a-= 365; }
    if (a>=365) { y++; a-= 365; } // up to 84, leap
    if (a>=1461) { y+= 4; a-=1461; }
    if (a>=1461) { y+= 4; a-=1461; }
    if (a>=1461) { y+= 4; a-=1461; }
    if (a>=1461) { y+= 4; a-=1461; } // up to 1600, leap...
    while (a>=146097) { y+= 400; a-=146097; } // number of days in 400 years, leap years included
    if (a>=36525) { y+= 100; a-=36525; } // number of days in 100 years with first year leap...
    if (a>=36524) { y+= 100; a-=36524; } // number of days in 100 years
    if (a>=36524) { y+= 100; a-=36524; } // number of days in 100 years
    while (a>=(1461-(halDateLeap(y)?0:1))) { a-=(1461-(halDateLeap(y)?0:1)); y+= 4; } // number of days in 4 years.. handle case where first year is not leap!
    while (a>=(365+(halDateLeap(y)?1:0))) { a-= (365+(halDateLeap(y)?1:0)); y++; }
    m=11;
    while ((uint32_t)a<(halNbDaysSince[m]+((m>=2)&&(halDateLeap(y)?1:0)))) m--;
    a= a - halNbDaysSince[m] - ((m>=2)&&(halDateLeap(y)?1:0));
    a++; // days are 1 based, not 0...
    m++; // same for months
    if (Y!=NULL) *Y= y; if (M!=NULL) *M= m; if (D!=NULL) *D= a;
    return true;
  }
static uint32_t volatile * const SNVS= (uint32_t volatile *)0x400D4000;
static uint32_t volatile * const HPCOMR= SNVS+(0x4/4);
static uint32_t volatile * const HPCR= SNVS+(0x8/4);
static uint32_t volatile * const HPSRTCMR= SNVS+(0x24/4);
static uint32_t volatile * const HPSRTCLR= SNVS+(0x28/4);
static uint32_t volatile * const CCRG5= (uint32_t volatile *)0x400FC07C;

  static uint64_t RTC_GetNow()
  {
    // get timer. since it is spread on 2 registers. one 32 bit one and one 15 bit ones, need to do 2 reads and make sure that they match
    uint64_t t1= (((uint64_t)*HPSRTCMR)<<32) + *HPSRTCLR; // Get "now"
    while (true) { uint64_t t2= (((uint64_t)*HPSRTCMR)<<32) + *HPSRTCLR; if (t1==t2) return t1; t1= t2; }
  }
void getDateTime(uint32_t *year, uint32_t *month, uint32_t *day, uint32_t *hour, uint32_t *min, uint32_t *sec, uint32_t *ms)
{
  uint64_t t1= RTC_GetNow();
  // 15 bit per seconds... use lower 15 bits for ms if needed
  if (ms!=NULL) *ms= (((uint32_t)t1 & 0x7fff) * 1000) >> 15; // get the number of ms if needed
  uint32_t tquot= (uint32_t)(t1>>15)/60; if (sec!=NULL) *sec= (uint32_t)(t1>>15)-tquot*60; // div mod nb seconds by 60 to separate seconds from the rest
  uint32_t tquot2= tquot/60; if (min!=NULL) *min= tquot-tquot2*60;               // div mod rest by 60 to get minutes and the rest
  tquot= tquot2/24; if (hour!=NULL) *hour= tquot2-tquot*24;                 // div mod rest by 24 to get hours and days since jan 1 2000
  // 152671 days from jan 1 1582 to jan 1 2000 (uncorrected for calendar change), which is what is used by DateFromDayIndex2 in napier
  halDateFromDayIndex2(152671+tquot, year, month, day);
}
void setDateTime(int32_t year, int32_t month, int32_t day, int32_t hour, int32_t min, int32_t sec)
{
  uint32_t y2, m2, d2, h2, M2, s2;
  getDateTime(&y2, &m2, &d2, &h2, &M2, &s2, NULL);
  if (year==-1) year= y2; if (month==-1) month= m2; if (day==-1) day= d2; if (hour==-1) hour= h2; if (min==-1) min= M2; if (sec==-1) sec= s2;
  uint64_t res= (halDateDayIndexFromDateinternal(day, month, year, halDateLeap(year))-152671)*24*60*60;
  res+= 60*60*hour + 60*min + sec;
  res<<=15;
  *HPCR&= ~1; while ((*HPCR&1)!=0);               // stop RTC
  *HPSRTCLR= (uint32_t)res; *HPSRTCMR= (uint32_t)(res>>32); // program registers
  *HPCR|= 1; while ((*HPCR&1)==0);                // re-enable RTC
}
void RTC_boot()
{
  *CCRG5|= 0xf<<27; // enables SNVS clocks
//    CCM->CCGR5 |= CCM_CCGR5_CG14(3);   //SNVS_HP_CLK_ENABLE
//    CCM->CCGR5 |= CCM_CCGR5_CG15(3);  //SNVS_LP_CLK_ENABLE
    *HPCOMR = (1U<<31); // enable all can read registers
//    SNVS->HPCR= SNVS_HPCR_BTN_CONFIG(3) | SNVS_HPCR_BTN_MASK_MASK; // ON button active on falling edge and ON button irq enabled
//    while ((SNVS->HPCR&SNVS_HPCR_BTN_MASK_MASK)==0); // wait until validated
    //SNVS->LPCR= (2<<20) | (3<<16) | (1<<5) | 1;   // Set ON_TIME delay, BTN debounce, dumb_pmic, clock enabled
    *HPCR|= 1;
    while ((*HPCR&1)==0); // make sure clock is on!
}


// MPU registers
// https://developer.arm.com/documentation/dui0646/b/cortex-m7-peripherals/optional-memory-protection-unit
  static uint32_t volatile * const MPU_TYPE= (uint32_t volatile *)0xE000ED90;
  static uint32_t volatile * const MPU_CTRL= (uint32_t volatile *)0xE000ED94;
  static uint32_t volatile * const MPU_RNR= (uint32_t volatile *)0xE000ED98;
  static uint32_t volatile * const MPU_RBAR= (uint32_t volatile *)0xE000ED9C;
  static uint32_t volatile * const MPU_RASR= (uint32_t volatile *)0xE000EDA0;
class CMPU { public:
  static int nbRegions() { return ((*MPU_TYPE)>>8)&0xff; }
  static void allowDefaultMap(bool allow) { if (allow) *MPU_CTRL|= 1<<2; else *MPU_CTRL&= ~(1<<2); }
  static bool allowedDefaultMap() { return (*MPU_CTRL&(1<<2))!=0; }
  static void enabled(bool allow) { if (allow) *MPU_CTRL|= 1; else *MPU_CTRL&= ~1; }
  static bool enabled() { return (*MPU_CTRL&1)!=0; }
  static void region(int r) { *MPU_RNR= (*MPU_RNR&~0xff)|r; }
  static int region() { return *MPU_RNR&0xff; }
  static uint32_t adr() { return *MPU_RBAR&~0x3f; }
  static void adr(uint32_t a) { *MPU_RBAR= a|(1<<5)|region(); }
  static void permission(bool &exec, int &ap, int &memaccess, bool &sharable, int &subregionOK, int &size, bool &enabled) 
  {
    uint32_t t= *MPU_RASR;
    exec= (t&(1<<28))!=0; ap= (t>>24)&0xf; memaccess= ((t>>16)&3) | ((t>>17)&0x1c);
    sharable= (t&(1<<18))!=0; subregionOK= (t>>8)&0x1f; 
    size= (t>>1)&0x1f; enabled= (t&1)!=0; 
  }
  static void setPermission(bool exec, int ap, int memaccess, bool sharable, int subregionOK, int size, bool enabled) 
  {
    *MPU_RASR= (exec?1<<28:0) | (ap<<24) | ((memaccess&0x1c)<<17) | ((memaccess&0x3)<<16) | (sharable?1<<18:0) | (subregionOK<<8) | (size<<1) | (enabled?1:0);
  }
  char const *apdecode(int ap)
  {
    static char const t[8][16]= {
      "Priv:NO /prv:No",
      "Priv:RW /prv:No",
      "Priv:RW /prv:RO",
      "Priv:RW /prv:RW",
      "Priv:?? /prv:??",
      "Priv:RO /prv:No",
      "Priv:RO /prv:Ro",
      "Priv:RO /prv:Ro"
    };
    return t[ap];
  }
  char const *memaccessdecode(int memaccess)
  {
    if (memaccess>11) return "Unknown";
    static char const * const t[]= {
      "Strong order",
      "device",
      "Norm write through",
      "Norm write back",
      "norm no cache",
      "reserved",
      "reserved",
      "norm: write back, write/read allocate",
      "device"
      "reserved",
      "reserved",
    };
    return t[memaccess];
  }
} MPU;

///////////////////////////////////////////////////
// Run code
void setup() 
{
  Serial.begin(9600);

  // Load setting from eeprom!
  setBackLight(70);
  touch.begin();
  tft.begin(); tft.setRotation(1); tft.useFrameBuffer(true);

  cameraBegin();
  RTC_boot();
}
 
CArduViseur *viseur= nullptr;
int count= 0;
void loop(void) 
{
  if (viseur==nullptr) viseur= new CArduViseur;
  char t[100];
  sprintf(t, "nb regions:%d %s %s", MPU.nbRegions(), MPU.allowedDefaultMap()?"allow defualt map":"", MPU.enabled()?"Enabled":"disabled"); Serial.println(t);
  for (int i=-1; i<MPU.nbRegions()+1; i++)
  {
    MPU.region(i);
    bool exec; int ap, memaccess; bool sharable; int subregionOK, size; bool enabled;
    MPU.permission(exec, ap, memaccess, sharable, subregionOK, size, enabled);
    sprintf(t, "r:%d @0x%lx %s perm:%s memaccess:%s %s subreg:%x size:%d %s", i, MPU.adr(), exec?"exec":"noexec", MPU.apdecode(ap), MPU.memaccessdecode(memaccess), sharable?"shared":"noshared", subregionOK, 1<<(size+1), enabled?"enabled":"disabled");
    Serial.println(t);
  }

  while (true)
  {
    viseur->tick(); 
    tft.updateScreenAsync(false); cameraRead(); tft.waitUpdateAsyncComplete(); // Scr update takes 100ms...
    //uint32_t h, m, s, ms; getDateTime(nullptr, nullptr, nullptr, &h, &m, &s, &ms); char t[100]; sprintf(t, "%ld:%ld:%ld %ld", h, m, s, ms); Serial.println(t);
  }
}
