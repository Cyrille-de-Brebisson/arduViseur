#include <stdint.h>
#include <string.h>
typedef uint16_t TPixel;

static float const MPI= 3.14159265358979323846264338327f;

/////////////////////////////////////
// This is the HAL part. These needs to be defined elsewhere
bool readTP(int32_t &x, int32_t &y);     // return false if up (and does not touch x/y), else returns true and x/y
int32_t setBackLight(int32_t v);         // v from 0 to 100. if <0, returns current back light
TPixel *getFB(int32_t &w, int32_t &h);   // returns framebuffer location and size
bool getCamera(uint8_t *&buf, int32_t &w, int32_t &h);    // reads the camera data and get the size. if buf==nullptr, will allocate a new one!
void debugOut(char const *t);            // write the string to debug
uint32_t getNow();                       // get the current time in ms
uint32_t getButtons();                   // get keyboard state... no need to debounce or anything
void saveData(uint8_t *d, int size);
bool loadData(uint8_t *d, int size);


/////////////////////////////////////
// Data structures. They are consts somewhere else
// TStarMore gives more info on a special star that might have a name, or known distances...
struct TStarMore { //6+n bytes. This is less than 5kb of size...
	uint8_t name:4, index1:4,
		index2:8,
		index3:5, bayerGreek1:3,
		bayerGreek2:2, bayerConst1:6,
		bayerConst2:1, dist1:7,
		dist2:3, iabsMag:5;
	TStarMore *next() const { return (TStarMore*)(name+((uint8_t const*)(this+1))); }
	int32_t index() const { return index1+(int32_t(index2)<<4)+(int32_t(index3)<<12); }
	int32_t bayerGreek() const { return bayerGreek1+(int32_t(bayerGreek2)<<3); }
	int32_t bayerConst() const { return bayerConst1+(int32_t(bayerConst2)<<6); }
	int32_t dist() const { return dist1+(int32_t(dist2)<<7); } // in light years
	int32_t absMag() const { return iabsMag-17; } // -17 to 15
	void getName(char *n) const { if (name==0) { *n=0; return; } memcpy(n, this+1, name); n[name]=' '; n[name+1]=0; }
	char const *getName() const { return (char const*)(this+1); }
	void setIndex(int32_t v) { index1= v; index2= v>>4; index3= v>>12; }
	void setBayerGreek(int32_t v) { bayerGreek1= v; bayerGreek2= v>>3; }
	void setBayerConst(int32_t v) { bayerConst1= v; bayerConst2= v>>6; }
	void setDist(int32_t v) { if (v>1023) v= 1023; dist1= v; dist2= v>>7; }
	void setAbsMag(int32_t v) { iabsMag= v+17; } // -17 to 15
};

// One star to display... or used for plate solving. includes only ra, dec, mag 
struct TStar { // 6 bytes. Could be dropped to 5 bytes by removing some precision if needed. Saves 100Kb of 600KB
	uint8_t ra1, ra2, ra3:5, dec1:3, dec2, dec3, dec4:2, imag:5, hasMoreInfo: 1;
	float ra() const { return float((ra1)+(int32_t(ra2)<<8)+(int32_t(ra3)<<16))*(2.0f*MPI/2097152.0f); } // in rad 0 to 2PI
	float dec() const { return float(dec1+(int32_t(dec2)<<3)+(int32_t(dec3)<<11)+(int32_t(dec4)<<19))*(MPI/2097152.0f); }  // in rad 0 to PI
	float mag() const { return (imag/2.0f)-2.0f; } 
	void setRa(float v) { uint32_t t= uint32_t(v*2097152.0/24.0); ra1=uint8_t(t); ra2=uint8_t(t>>8); ra3=uint8_t((t>>16)&0x1f); } // ra in 0-24 range
	void setDec(float v) { uint32_t t= uint32_t((v+90.0)*2097152.0/180.0); dec1=uint8_t(t&0x7); dec2=uint8_t(t>>3); dec3=uint8_t(t>>11); dec4=uint8_t(t>>19); } // dec in -90 to 90 range
	void setMag(float v) { if (v<-2) v= -2; if (v>13.5) v=13.5; imag= uint8_t((v+2)*2); } // from -2 to 13.5
	TStarMore const *moreInfo() const;
    TStarMore const *moreInfo(int32_t starIndex) const;
};

// for display of constellation asterism. This is a list that contains the id in the TStar list of the first star of the link
// The 2nd star is only given as a positive offset from the first to save space.
// The list is sorted for fast use
// 646 items in it. around 2.5kb
struct TStarLink { uint32_t first:17, delta:15; };

// This is for MEssier and Caldwell catalogs. includes for each object the position, type, magnitude, size and distance.
// If it has a name, then an index in a string array is also provided...
struct TSkyCatalog { uint16_t ra; int16_t dec;  // small in size, <5kb each
	float getra() const { return ra/2048.0f/24.0f*2.0f*MPI; }
	float getdec() const { return dec/256.0f/180.0f*MPI; }
	uint32_t tpe: 4, mag:4, szeM:7, szee:2, dstM:7, dste:3, strindex: 6; 
	char const *getString(char const * const *strs) const { if (strindex==0) return nullptr; return strs[strindex-1]; }
	float getSze() const { if (szee==0) return szeM/10.0f; if (szee==1) return float(szeM); if (szee==2) return szeM*10.0f; return szeM/1000.0f; }
	int32_t getSze(int &ip) const { if (szee==0) { ip=szeM/10; return szeM%10; }
	                            if (szee==1) ip=szeM; else if (szee==2) ip=szeM*10; else ip=szeM*100; 
								return 0; }
	int32_t getDst() const { int r= dstM; for (int i=dste; --i>=0;) r*=10; return r; }
};

///////////////////////////////////////////////////////
// Our main display/controler class! does everything!
class CArduViseur {
public:
	enum { dmSkyCameraInsert, dmCameraSkyInsert, dmCamera, dmSky, dmSkyCamera };
	uint32_t displayMode: 3;       // one of the above
	uint32_t track: 1;             // lock sky with camera?
	uint32_t objectSelected: 1;    // true if object is selected and we need to show arrows. and also the unselect button!
	uint32_t findMode: 3;          // 0: no find, 1: M, 2: C, 3: starname, 4: star const
	int32_t findModeM=0, findModeC=0, findModestarName=0, findModeStarConst=0, findModeStarConstAlpha=0;
	int32_t findModeMax= 0;
	float skyra= MPI/2.0f, skydec=73*3.14f/180.0f, skyrot=0.0f, platea=0.0f, plated=0.0f, plater=0.0f;  // where we look at in the sky, what the camera knows...
	float destDec, destRa; bool pointToDest= false;
	int viewAnglei=0; // 0=50 1=40 2=30 3=20 4=10 5=5 6=2 7=1
	void setSkyra(float ra) { skyra= ra; resetlimMagFind(); }
	void setSkydec(float dec) { skydec= dec; resetlimMagFind(); }
	void setViewAnglei(int i) { viewAnglei= i; resetlimMagFind(); }

	// where to display the inserts in display insert modes...
	int32_t const camInsertH= 80, camInsertW= 80;
	int32_t const skyInsertH= 80, skyInsertW= 80;

	////////////////////////////////////////
	// Framebuffer functions...
	TPixel *fb=nullptr; int32_t screenw=0, screenh=0, clipx=0, clipX=0, clipy=0, clipY=0; // here is the framebuffer, its size, and the cliping region for the clipped version of the drawing primitives
	// sacve/restore clip allows a function to save the current clipping region (in a 4 uint32_t array) and restore it later...
	void saveClip(int32_t s[4], int32_t x, int32_t y, int32_t X, int32_t Y) { memcpy(s, &clipx, sizeof(clipx)*4); clipx= x, clipX= X, clipy= y, clipY= Y; }
	void updateClip(int32_t s[4], int32_t x, int32_t y, int32_t X, int32_t Y) 
	{ 
		memcpy(s, &clipx, sizeof(clipx)*4); 
		if (clipx<x) clipx= x; if (clipX>=X) clipX= X;
		if (clipy<y) clipy= y; if (clipY>=Y) clipY= Y;
	}
	void restoreClip(int32_t const s[4]) { memcpy(&clipx, s, sizeof(clipx)*4); }
	// here we have the clipped versions of the drawing primitives
	void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, TPixel c);
	void rect(int32_t x, int32_t y, int32_t w, int32_t h, TPixel c);
	void rect(int32_t x, int32_t y, int32_t w, int32_t h, TPixel c, TPixel cin);
	void hLine(int32_t x, int32_t y, int32_t w, TPixel c);
	void vLine(int32_t x, int32_t y, int32_t h, TPixel c);
	void line(float x1, float y1, float x2, float y2, TPixel c);
	void pixel(int32_t x, int32_t y, TPixel c);
	void blit(int32_t x, int32_t y, TPixel const *b, int32_t w, int32_t h, TPixel trensparent);
	void blit(int32_t x, int32_t y, TPixel const *b, int32_t w, int32_t h);
	bool lightUp(int32_t x, int32_t y, uint8_t const *b, int32_t w, int32_t h, uint32_t filter= 7);
	// and here the unclipped versions!
	void inline hLine2(int32_t x, int32_t y, int32_t w, TPixel c) { TPixel *d= fb+x+y*screenw; while (--w>=0) *d++= c; }
	void inline vLine2(int32_t x, int32_t y, int32_t h, TPixel c) { TPixel *d= fb+x+y*screenw; while (--h>=0) { *d= c; d+=screenw; } }
	void inline pixel2(int32_t x, int32_t y, TPixel c) { fb[x+y*screenw]= c; }
	void line2(int32_t x1, int32_t y1, int32_t x2, int32_t y2, TPixel c);
	// WARNING, TEXT DRAWING IS UNCLIPPED!!!!
	void text(int32_t x, int32_t y, int32_t w, char const *t, TPixel fg, TPixel bg);
	void text(int32_t x, int32_t y, int32_t w, char const *t, TPixel fg);
	void textCenter(int32_t x, int32_t y, char const *t, TPixel fg) { int l= int(strlen(t)); text(x-l*3,y-4, l*6, t, fg); }
	void textClip(int32_t x, int32_t y, char const *t, TPixel fg); // x clip is not perfect BTW!


	//////////////////////////////////////////////
	// Constructor/init
	CArduViseur(): displayMode(dmSkyCameraInsert), track(true), objectSelected(false), findMode(0)
	{
		fb= getFB(screenw, screenh); clipX= screenw, clipY= screenh;
		displayText[0]= 0;
    if (!loadData((uint8_t*)&tpCal, sizeof(tpCal))) tpCal.init();
	}

	////////////////////////////////////////////
	// use functions...
	void draw();
	void drawCamera(int32_t x, int32_t y, bool zoom);
	  int32_t limMag=10; bool findLimMag=true;
	  void resetlimMagFind() { limMag=10, findLimMag= true; }
	  // This has do do with trying to easely locate objects that have been displayed. on each display, the location of items of interest will be save here...
	  struct TDisplayObjType { uint16_t x, y; uint32_t index: 30, type:2; } displayObjs[100];
	  int nbDisplayObjs=0;
	  void addDspObj(uint32_t x, uint32_t y, uint32_t index, uint32_t type)
	  {
		  if (nbDisplayObjs==100) return;
		  displayObjs[nbDisplayObjs].x= x, displayObjs[nbDisplayObjs].y= y;
		  displayObjs[nbDisplayObjs].index= index; displayObjs[nbDisplayObjs++].type= type;
	  }
	  TDisplayObjType *findDspObj(int32_t x, int32_t y);
	  char displayText[54]=""; // text to display at the bottom of the screen
	void drawSky(int32_t x, int32_t y, int32_t w, int32_t h);

	    struct TTextAndPos { 
			char const *t; int8_t tsize; char prefix1, sufix; int8_t index; float dec, ra; int16_t x, y, X, Y;
			void init(char const *t, int32_t tsize, char prefix1, char sufix, int32_t index, float dec, float ra)
			{ this->t= t; if (tsize==-1) this->tsize= t==nullptr ? 0 : int8_t(strlen(t)); else this->tsize= tsize; this->prefix1= prefix1; this->sufix= sufix; 
			  this->index= int8_t(index); this->dec= dec; this->ra= ra; }
			static int comp(void const *A, void const *B) 
			{ TTextAndPos const *a= (TTextAndPos const *)A, *b= (TTextAndPos const *)B;
				if (a->prefix1!=b->prefix1) return a->prefix1-b->prefix1; else return a->sufix-b->sufix; }
		} *drawFindTextPos= nullptr; 
		uint32_t drawFindTextPosNb= 0;
		void generateTextAndPos();
		void centerRectText(int32_t x, int32_t y, int32_t w, int32_t h, char const *t, int32_t p, TPixel c);
		bool inCenterRectText(int32_t px, int32_t py, int32_t x, int32_t y, int32_t w, int32_t h, int32_t p);
	void drawFind(int x, int y, int w, int h);

	struct TLastSky { int32_t x, y, w, h; float scale, cosSkyd, sinSkyd, cosSkyrot, sinSkyrot; } lastSky;
	bool raDecToXY(float ra, float dec, float &x, float &y);
	bool raDecToXY(float ra, float dec, int32_t &x, int32_t &y) { float fx, fy; if (!raDecToXY(ra, dec, fx, fy)) return false; x= int32_t(fx), y= int32_t(fy); return true; }
	bool XYToraDec(int32_t x, int32_t y, float &ra, float &dec);
	bool SkyForXYRaDec(int32_t x, int32_t y, float ra, float dec);

      int32_t penx=0, peny=0; int32_t penInitx=0, penInity=0; bool penDrag=false, penDown=false;
	void penEvent(); // pen event contacts the lower layer and generates clicks and drag calls...
	struct TtpCal { int32_t a, b, c, d, e, f; uint8_t initStage, t1, t2, crc; TtpCal() { init(); } void init() { a=256, b= c= d= 0; e=256, f= 0; initStage= 0; } } tpCal;
	int32_t calT[6]; 
	void touchToPoint(int32_t &x, int32_t &y);
	bool touchToPointInit(int32_t const *P, int32_t const *T);
	void clickCalibration(int32_t x, int32_t y);
	void drawCalibration();

	void click(int32_t x, int32_t y);
	float skydownra, skydowndec;
	void drag(int32_t x, int32_t y, int32_t dx, int32_t dy);
	void keyPress(int keyId);
	uint32_t lastKeyboard= 0, lastKeyPressTime= 0; int32_t lastKeyDown= -1; 
	void handleKeys();

	uint8_t *camImg=nullptr; int32_t camW=0, camH=0;
	void solve(); // calculates platea/d/r from camImg

	void tick(); // call reguallry, will call penEvent, redraw and handle all user events... will also get a new camera image and solve

	// stats: how long did these take?
	uint32_t lastDraw=0, lastCamera=0, lastSolve=0;
};
