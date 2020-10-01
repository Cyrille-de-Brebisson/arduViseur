// Note: it is important for the pins to be #defined here to allow the fast reads to work properly as the need to know the pins at compile time
#define PinCamVsync 2  // EMC_04
#define PinCamHref  3  // EMC_05
#define PinCamPpclk 4  // EMC_06
#define PinCamXclk  5  // EMC_08
#define PinCamD0    14 // AD_B1_02
#define PinCamD1    15 // AD_B1_03
#define PinCamD2    17 // AD_B1_06
#define PinCamD3    16 // AD_B1_07
#define PinCamD4    22 // AD_B1_08
#define PinCamD5    23 // AD_B1_09
#define PinCamD6    20 // AD_B1_10
#define PinCamD7    21 // AD_B1_11
#include "Camera.h"

#include <Wire.h>
static int const cameraAddress= 0x21; // i2c address of camera's sccb device
static struct { uint8_t reg, val; } const OV7670Registers[] = {
  {0x12, 0x80}, // Reset to default values
  {0x11, 0x80}, // Set some reserved bit to 1!
  {0x3B, 0x0A}, // Banding filter value uses BD50ST 0x9D as value. some reserved stuff + exposure timing can be less than limit on strong light
  {0x3A, 0x04}, // output sequense elesection. Doc too lmited to be sure
  {0x3A, 0x04},
  {0x12, 0x04}, // Output format: rgb
  {0x8C, 0x00}, // Disable RGB444
  {0x40, 0xD0}, // Set RGB565
  {0x17, 0x16}, // Y window start msb (3-11) I think
  {0x18, 0x04}, // Y window end msb (3-11)
  {0x32, 0x24}, // Y window lsb end= 100b start= 100b
  {0x19, 0x02}, // X window start msb (2-10) I think
  {0x1A, 0x7A}, // X window end msb (2-10) I think
  {0x03, 0x0A}, // X window lsb (10 and 10)
  {0x15, 0x02}, // VSync negative
  {0x0C, 0x04}, // DCW enable? 
  {0x3E, 0x1A}, // Divide by 4
  {0x1E, 0x27}, // mirror image black sun enabled and more reserved!
  {0x72, 0x22}, // Downsample by 4
  {0x73, 0xF2}, // Divide by 4
  {0x4F, 0x80}, // matrix coef
  {0x50, 0x80},
  {0x51, 0x00},
  {0x52, 0x22},
  {0x53, 0x5E},
  {0x54, 0x80},
  {0x56, 0x40}, // contracts
  {0x58, 0x9E}, // matrix
  {0x59, 0x88}, // AWB
  {0x5A, 0x88},
  {0x5B, 0x44},
  {0x5C, 0x67},
  {0x5D, 0x49},
  {0x5E, 0x0E},
  {0x69, 0x00}, // gain per channel
  {0x6A, 0x40}, // more gain
  {0x6B, 0x0A}, // pll reserved stuff!
  {0x6C, 0x0A}, // AWB
  {0x6D, 0x55},
  {0x6E, 0x11},
  {0x6F, 0x9F},
  {0xB0, 0x84}, // reserved!
  {0xFF, 0xFF}  // End marker
};

// Read a single uint8_t from address and return it as a uint8_t
static uint8_t cameraReadRegister(uint8_t address) 
{
  Wire.beginTransmission(cameraAddress);
  Wire.write(address);
  Wire.endTransmission();

  Wire.requestFrom(cameraAddress, 1);
  uint8_t data = Wire.read();
  Wire.endTransmission();
  return data;
}

// Writes a single uint8_t (data) into address
static int cameraWriteRegister(uint8_t address, uint8_t data) 
{
  Wire.beginTransmission(cameraAddress);
  Wire.write(address); Wire.write(data);
  return Wire.endTransmission();
}

// Reset all OV7670 registers to their default values
static void cameraReset() 
{
  cameraWriteRegister(0x12, 0x80); delay(10);
  cameraWriteRegister(0x12, 0); delay(10);
}

// Read and display all of the OV7670 registers
static void cameraReadI2C() 
{
  for (int i = 0; i <= 0xC9; i ++) 
  {
    Serial.print("Register: "); Serial.print(i, HEX); Serial.print(" Value: "); uint8_t reg = cameraReadRegister(i); Serial.println(reg, HEX);
  }
}

// Slow the frame rate enough for camera code to run with 8 MHz XCLK. Approximately 1.5 frames/second
static void cameraSlowFrameRate() 
{ // Clock = external-clock * pll_mull(0,4,6 or 8 on bits 6 and 7) / (2*divider_5_lsb+1)
  // Here: 
  // CLKRC register: Prescaler divide by 31
  uint8_t reg = cameraReadRegister(0x11); cameraWriteRegister(0x11, (reg & 0b1000000) | 0b00011110);
  // DBLV register: PLL = 6
  reg = cameraReadRegister(0x6B); cameraWriteRegister(0x6B, (reg & 0b00111111) | 0b10000000);
}
static void cameraClock(int mhz) // Assumes pixel clock = 8Mhz
{ // Clock = external-clock * pll_mull(0,4,6 or 8 on bits 6 and 7) / (2*divider_5_lsb+1)
  float ratio= mhz/8.0f; // this is what we want to get to... find closest value...
  float const pll[3]= { 4.0f, 6.0f, 8.0f };
  int bestpll= 0, bestdiv=0; float bestr= 1000.0;
  for (int p= 0; p<3; p++)
    for (int d=0; d<32; d++)
    {
      float r= pll[p]/(2*(d+1)); if (abs(r-ratio)<abs(ratio-bestr)) { bestr= r; bestpll= p+1; bestdiv= d; }
    }
   
  // CLKRC register: Prescaler divide by 31
  uint8_t reg = cameraReadRegister(0x11); cameraWriteRegister(0x11, (reg & 0b1000000) | bestdiv);
  // DBLV register: PLL = 6
  reg = cameraReadRegister(0x6B); cameraWriteRegister(0x6B, (reg & 0b00111111) | (bestpll<<6));
  char t[100]; sprintf(t, "camera clock %f with pll=%d and div=%d", 8.0f*bestr, bestpll, bestdiv); Serial.println(t);
}

static void cameraFrameStartIrq();
static void cameraReadLineIrq();
static inline bool getCamVsync() { return (GPIO4_DR & (1<<4))!=0; } // 2  // EMC_04
static inline bool getCamHref() { return (GPIO4_DR & (1<<5))!=0; } // 3  // EMC_05
static void inline cameraStopLineIrq() { GPIO4_IMR&= ~(1<<5); } // disable irq
static void inline cameraStartLineIrq() { GPIO4_ISR&= ~(1<<5); GPIO4_IMR|= 1<<5; } // clear any previous IRQ and re-enabled
static inline bool getCamPpclk() { return (GPIO4_DR & (1<<6))!=0; } // 4  // EMC_06

void cameraBegin()
{
    // Setup all GPIO pins as inputs
    pinMode(PinCamVsync, INPUT); pinMode(PinCamHref,  INPUT); pinMode(PinCamPpclk,  INPUT);
    pinMode(PinCamD0,    INPUT); pinMode(PinCamD1,    INPUT); pinMode(PinCamD2,    INPUT); pinMode(PinCamD3,    INPUT); pinMode(PinCamD4,    INPUT); pinMode(PinCamD5,    INPUT); pinMode(PinCamD6,    INPUT); pinMode(PinCamD7,    INPUT);
    analogWriteFrequency(PinCamXclk, 8000000); analogWrite(PinCamXclk, 127); delay(100); // 9mhz works, but try to reduce to debug timings with logic analyzer
    // Configure the camera for operation
    Wire.begin();
    int i= 0; while (OV7670Registers[i].reg!=0xff) { cameraWriteRegister(OV7670Registers[i].reg, OV7670Registers[i].val), i++; }
    cameraWriteRegister(0x3a, 0x14);cameraWriteRegister(0x67, 0xc80);cameraWriteRegister(0x68, 0x80); // B&W mode...
    //cameraClock(12);
    //cameraWriteRegister(0x11, 0x07); cameraWriteRegister(0x3b, 0x0a);// Night mode1
    //cameraWriteRegister(0x11, 0x03); cameraWriteRegister(0x3b, 0x0a);// Night mode2
    delay(100);
    attachInterrupt(digitalPinToInterrupt(PinCamVsync), cameraFrameStartIrq, RISING);
    attachInterrupt(digitalPinToInterrupt(PinCamHref), cameraReadLineIrq, RISING); //cameraStopLineIrq();

}

static inline uint32_t cameraReadPixel() 
{
  uint32_t pword= GPIO6_DR >> 18;  // get the port bits. We want bits 18, 19 and 22 to 27
  return (pword&3) | ((pword&0x3f0)>>2);
}

static uint8_t cameraImageTemp[ccameraHeight*ccameraWidth]; // QQVGA image buffer in B&W format format. This is where the IRQ stores the incomming picture
uint8_t cameraImage[ccameraHeight*ccameraWidth];            // When an image is complete, it is copied here (note: use switch buffers to avoid mem copy?)
static bool cameraNewFrame= false;          // true when cameraImage contains a new image that was never read
static uint32_t cameraPixelPos= 0, cameraLineCount= 0; // Used to know where to store the nex incmming pixel and where we are in the frame receiving...
static uint32_t nextFrameGatherTime= 0;     // when next to grab a frame? This is used to limit the framerate
static bool cameraframeError= false;        // true if an error was detected
static bool cameraframeDone= true;          // true when a frame was fully received. no further reception will be done while ture

// Function called on line sync raising IRQ. Reads an incomming line and stores it in cameraImageTemp
static void cameraReadLineIrq()
{
  if (cameraframeDone) return;           // not actively receiving. Ignore the frame.
//  uint32_t linet1= ARM_DWT_CYCCNT;
  for (int k=cameraWidth(); --k>=0;)     // get a full ine
  {
    if (!getCamHref() || !getCamVsync()) goto er;// if premature end detected. note it.
    int i= 200; while (!getCamPpclk()) if (--i==0) goto er; // Wait for clock to go high
    uint32_t hByte = cameraReadPixel();
    i=200; while (getCamPpclk()) if (--i==0) goto er;   // Wait for clock to go back low
    i= 200; while (!getCamPpclk()) if (--i==0) goto er; // Wait for clock to go high
    uint32_t lByte = cameraReadPixel();
    cameraImageTemp[cameraPixelPos++] = (((hByte << 8) | lByte) >> 5)&63; // save data.
    i=200; while (getCamPpclk()) if (--i==0) goto er;   // Wait for clock to go back low
  }
  if (--cameraLineCount>0) return;      // frame not done? ready for next line!
  cameraframeDone= true;                // stop further reading until requested
  cameraNewFrame= true;                 // note that the frame is complete for the caller
  memcpy(cameraImage, cameraImageTemp, sizeof(cameraImageTemp)); // Copy it to main buffer
  return;
  er:
  cameraframeError= true; return;
}

// Function called on frame sync raising IRQ. schedule a read of the next frame.
static void cameraFrameStartIrq()
{
  // limit frame reading at 5/s
  uint32_t now= millis();
  if (nextFrameGatherTime>now) return;
  nextFrameGatherTime= now+200;        // schedule next read in 200ms
  // ready for next frame...
  cameraPixelPos= 0; cameraLineCount= cameraHeight(); cameraframeError= false; cameraframeDone= false;
}

// return true if a new frame is available in the cameraImage buffer.
bool cameraRead()
{
  bool r= cameraNewFrame; cameraNewFrame= false; return r;
}
