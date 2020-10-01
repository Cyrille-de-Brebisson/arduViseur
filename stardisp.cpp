// Jean-Luc Starck for image processing..
// OV7725

// Add object graphics for catalog objects?
// Add greek letters to 48 font

// Plan
// 4: setup camera, show image to screen
// 5: astrometry
//   1: clear image
//   2: solve

// UI:
// track/untrack toggle
// click on object to select. then shows arrows and distance
// find UI (with images?)
// back light control
// buttons: mode: camera, sky, camera/sky, camera+sky insert, sky+camera insert
//         track/untrack (locks sky with camera)
//         unselect object (or just allow to clik anywhere)?
//         find (F3)
//         backlight
// RTC for planets?

#include "stardisp.h"
#ifdef ARDUINO
#include <Arduino.h>
#define debug(...) { char debt[200]; sprintf(debt, __VA_ARGS__); debugOut(debt); }
#else
template <typename T> T abs(T a) { if (a>0) return a; return -a; }
#define PROGMEM
#include <qdebug.h>
#define debug(...) { char debt[200]; sprintf(debt, __VA_ARGS__); qDebug() << debt; }
#endif
#include "stars.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>


static float const degToRad = MPI/180.0f;
#define dtr(a) (float(a)*degToRad)

char const bayerGreek[24][4]= {{"Alp"},{"Bet"},{"Chi"},{"Del"},{"Eps"},{"Eta"},{"Gam"},{"Iot"},{"Kap"},{"Lam"},{"Mu"},{"Nu"},{"Ome"},{"Omi"},{"Phi"},{"Pi"},{"Psi"},{"Rho"},{"Sig"},{"Tau"},{"The"},{"Ups"},{"Xi"},{"Zet"}};
char const bayerConst[88][4]= {{"And"},{"Ant"},{"Aps"},{"Aql"},{"Aqr"},{"Ara"},{"Ari"},{"Aur"},{"Boo"},{"CMa"},{"CMi"},{"CVn"},{"Cae"},{"Cam"},{"Cap"},{"Car"},{"Cas"},{"Cen"},{"Cep"},{"Cet"},{"Cha"},{"Cir"},{"Cnc"},{"Col"},{"Com"},{"CrA"},{"CrB"},{"Crt"},{"Cru"},{"Crv"},{"Cyg"},{"Del"},{"Dor"},{"Dra"},{"Equ"},{"Eri"},{"For"},{"Gem"},{"Gru"},{"Her"},{"Hor"},{"Hya"},{"Hyi"},{"Ind"},{"LMi"},{"Lac"},{"Leo"},{"Lep"},{"Lib"},{"Lup"},{"Lyn"},{"Lyr"},{"Men"},{"Mic"},{"Mon"},{"Mus"},{"Nor"},{"Oct"},{"Oph"},{"Ori"},{"Pav"},{"Peg"},{"Per"},{"Phe"},{"Pic"},{"PsA"},{"Psc"},{"Pup"},{"Pyx"},{"Ret"},{"Scl"},{"Sco"},{"Sct"},{"Ser"},{"Sex"},{"Sge"},{"Sgr"},{"Tau"},{"Tel"},{"TrA"},{"Tri"},{"Tuc"},{"UMa"},{"UMi"},{"Vel"},{"Vir"},{"Vol"},{"Vul"}};

TStarMore const *TStar::moreInfo() const { return moreInfo(int32_t(this-stars)); }
TStarMore const *TStar::moreInfo(int32_t starIndex) const
{ 
    if (!hasMoreInfo) return nullptr; 
    TStarMore const *s= (TStarMore const*)starsMore;
    for (int i= nbMoreStars; --i>=0; ) if (s->index()==starIndex) return s; else s= s->next();
    return nullptr;
}

void CArduViseur::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, TPixel c)
{
    if (y+h<clipy || y>=clipY) return;
    if (x+w<clipx || x>=clipX) return;
    if (x<clipx) { w-=clipx-x; x=clipx; }
    if (y<clipy) { h-=clipy-y; y=clipy; }
    if (w>=clipX-x) w= clipX-x;
    if (h>=clipY-y) h= clipY-y;
    if (w<=0) return;
    if (h<=0) return;
    while (--h>=0) hLine2(x, y++, w, c);
}
void CArduViseur::rect(int32_t x, int32_t y, int32_t w, int32_t h, TPixel c)
{
    if (h<=0 || w<=0) return;
    hLine(x, y, w, c);
    if (h>1) hLine(x, y+h-1, w, c);
    if (h<2) return;
    vLine(x, y+1, h-2, c);
    if (w>1) vLine(x+w-1, y+1, h-2, c);
}
void CArduViseur::rect(int32_t x, int32_t y, int32_t w, int32_t h, TPixel c, TPixel cin)
{
    rect(x, y, w, h, c);
    fillRect(x+1, y+1, w-2, h-2, cin);
}
void CArduViseur::hLine(int32_t x, int32_t y, int32_t w, TPixel c)
{
    if (y<clipy || y>=clipY) return;
    if (x<clipx) { w-=clipx-x; x= clipx; }
    if (w>=clipX-x) w= clipX-x-1;
    if (w<0) return;
    hLine2(x, y, w, c);
}
void CArduViseur::vLine(int32_t x, int32_t y, int32_t h, TPixel c)
{
    if (x<clipx || x>=clipX) return;
    if (y<clipy) { h-=clipy-y; y=clipy; }
    if (h>=clipY-y) h= clipY-y-1;
    if (h<0) return;
    vLine2(x, y, h, c);
}
void CArduViseur::pixel(int32_t x, int32_t y, TPixel c)
{
    if (y<clipy || y>=clipY || x<clipx || x>=clipX) return;
    pixel2(x, y, c);
}
void CArduViseur::blit(int32_t x, int32_t y, TPixel const *b, int32_t w, int32_t h, TPixel trensparent)
{
    TPixel *d= fb+x+y*screenw;
    for (int32_t Y=0; Y<h; Y++)
    {
        int32_t ty= y+Y; if (ty<clipy || ty>=clipY) continue;
        for (int32_t X=0; X<w; X++)
        {
            int32_t tx= x+X; TPixel c= *b++;
            if (tx<clipx || tx>=clipX || c==trensparent) continue;
            d[X]= c;
        }
        d+= screenw;
    }
}
void CArduViseur::blit(int32_t x, int32_t y, TPixel const *b, int32_t w, int32_t h)
{
    TPixel *d= fb+x+y*screenw;
    for (int32_t Y=0; Y<h; Y++)
    {
        int32_t ty= y+Y; if (ty<clipy || ty>=clipY) continue;
        for (int32_t X=0; X<w; X++)
        {
            int32_t tx= x+X; TPixel c= *b++;
            if (tx<clipx || tx>=clipX) continue;
            d[X]= c;
        }
        d+= screenw;
    }
}
bool CArduViseur::lightUp(int32_t x, int32_t y, uint8_t const *b, int32_t w, int32_t h, uint32_t filter)
{
    if (x+w<=clipx || x>=clipX) return false;
    if (y+h<=clipy || y>=clipY) return false;
    TPixel *d= fb+x+y*screenw;
    for (int32_t Y=0; Y<h; Y++)
    {
        for (int32_t X=0; X<w; X++)
        {
            int32_t tx= x+X, ty= y+Y; uint8_t c= *b++; d++;
            if (ty<clipy || ty>=clipY|| tx<clipx || tx>=clipX) continue;
            TPixel p= d[-1];
            uint32_t R= (p&0x1f); if ((filter&1)!=0) R+=c; if (R>0x1f) R= 0x1f;
            uint32_t G= ((p>>5)&0x3f); if ((filter&2)!=0) G+=c*2; if (G>0x3f) G= 0x3f;
            uint32_t B= ((p>>11)&0x1f); if ((filter&4)!=0) B+=c; if (B>0x1f) B= 0x1f;
            d[-1]= R|(G<<5)|(B<<11);
        }
        d+= screenw-w;
    }
    return true;
}
    // update x1 and y1 so that x1=clip and y1 is proportional as needed
    static void lclip(float &x1, float x2, float clip, float &y1, float y2)
    {
        y1= (y2-y1)*(clip-x1)/(x2-x1)+y1;
        x1= clip;
    }
void CArduViseur::line(float x1, float y1, float x2, float y2, TPixel c)
{
    if (x1<clipx && x2>=clipx) lclip(x1, x2, float(clipx), y1, y2);
    if (x2<clipx && x1>=clipx) lclip(x2, x1, float(clipx), y2, y1);
    if (x1<clipX && x2>=clipX) lclip(x2, x1, float(clipX-1), y2, y1);
    if (x2<clipX && x1>=clipX) lclip(x1, x2, float(clipX-1), y1, y2);
    if (y1<clipy && y2>=clipy) lclip(y1, y2, float(clipy), x1, x2);
    if (y2<clipy && y1>=clipy) lclip(y2, y1, float(clipy), x2, x1);
    if (y1<clipY && y2>=clipY) lclip(y2, y1, float(clipY-1), x2, x1);
    if (y2<clipY && y1>=clipY) lclip(y1, y2, float(clipY-1), x1, x2);
    if ((x1<clipx && x2<clipx) || (x1>=clipX && x2>=clipX)) return;
    if ((y1<clipy && y2<clipy) || (y1>=clipY && y2>=clipY)) return;
    line2(int32_t(x1), int32_t(y1), int32_t(x2), int32_t(y2), c);
}

void CArduViseur::line2(int32_t x1, int32_t y1, int32_t x2, int32_t y2, TPixel c)
{
    int32_t dx= abs(x2-x1), dy= abs(y2-y1);
    if (dx==0) { if (y1<y2) vLine(x1, y1, y2-y1, c); else vLine(x1, y2, y1-y2, c); return; }
    if (dy==0) { if (x1<x2) hLine(x1, y1, x2-x1, c); else hLine(x2, y1, x1-x2, c); return; }
    if (dy<=dx) 
    {
        if (x1>x2) { int32_t t= x1; x1= x2; x2= t; t= y1; y1= y2; y2= t; }
        TPixel *p= fb+x1+y1*screenw;
        int32_t err= dx/2;
        int32_t ystep= y1<y2? screenw : -screenw;
        int32_t x= dx; while (--x>=0)
        {
            *p++= c;
            err-= dy; if (err<0) { err+= dx; p+=ystep; }
        }
    } else {
        if (y1>y2) { int32_t t= x1; x1= x2; x2= t; t= y1; y1= y2; y2= t; }
        TPixel *p= fb+x1+y1*screenw;
        int32_t err= dy/2;
        int32_t xstep= x1<x2? 1 : -1;
        int32_t y= dy; while (--y>=0)
        {
            *p= c; p+= screenw;
            err-= dx; if (err<0) { p+=xstep; err+= dy; }
        }
    }
}

static uint64_t const font8[]=
{ 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000000000000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0101000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 
0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x0000E0E0E0000000ULL, 0x80C0E1F1E1C08000ULL, 0xF1F1F1F1F1F1F100ULL, 0x0000000000515100ULL, 0x0000000000515100ULL, 
0x0000000000000000ULL, 0x4040404040004000ULL, 0xA0A0A00000000000ULL, 0xA0A0F1A0F1A0A000ULL, 0x40E150E041F04000ULL, 0x3031804020918100ULL, 0x2050502051906100ULL, 0x4040400000000000ULL, 0x8040202020408000ULL, 0x2040808080402000ULL, 0x00A040F140A00000ULL, 0x004040F140400000ULL, 0x0000000060604020ULL, 0x000000F100000000ULL, 0x0000000000606000ULL, 0x0001804020100000ULL, 
0xE01191513111E000ULL, 0x406040404040E000ULL, 0xE01101C02010F100ULL, 0xE01101E00111E000ULL, 0x80C0A090F1808000ULL, 0xF110F0010111E000ULL, 0xC02010F01111E000ULL, 0xF101804020202000ULL, 0xE01111E01111E000ULL, 0xE01111E101806000ULL, 0x0060600060600000ULL, 0x0060600060604020ULL, 0x8040201020408000ULL, 0x0000F100F1000000ULL, 0x1020408040201000ULL, 0xE011018040004000ULL, 
0xE01151D15010E100ULL, 0xE01111F111111100ULL, 0xF01111F01111F000ULL, 0xE01110101011E000ULL, 0x7090111111907000ULL, 0xF11010F01010F100ULL, 0xF11010F010101000ULL, 0xE01110109111E100ULL, 0x111111F111111100ULL, 0xE04040404040E000ULL, 0x010101011111E000ULL, 0x1190503050901100ULL, 0x101010101010F100ULL, 0x11B1515111111100ULL, 0x1111315191111100ULL, 0xE01111111111E000ULL, 
0xF01111F010101000ULL, 0xE011111151906100ULL, 0xF01111F050901100ULL, 0xE01110E00111E000ULL, 0xF140404040404000ULL, 0x111111111111E000ULL, 0x111111A0A0404000ULL, 0x1111115151B11100ULL, 0x1111A040A0111100ULL, 0x1111A04040404000ULL, 0xF10180402010F100ULL, 0xE02020202020E000ULL, 0x0010204080010000ULL, 0xE08080808080E000ULL, 0x40A0110000000000ULL, 0x000000000000F100ULL, 
0x2020400000000000ULL, 0x0000E001E111E100ULL, 0x1010F0111111F000ULL, 0x0000E1101010E100ULL, 0x0101E1111111E100ULL, 0x0000E011F110E000ULL, 0x40A0207020202000ULL, 0x0000E01111E101E0ULL, 0x1010F01111111100ULL, 0x400060404040E000ULL, 0x8000C08080809060ULL, 0x1010905030509000ULL, 0x604040404040E000ULL, 0x0000B05151511100ULL, 0x0000F01111111100ULL, 0x0000E0111111E000ULL, 
0x0000F01111F01010ULL, 0x0000E11111E10101ULL, 0x0000D13010101000ULL, 0x0000E110E001F000ULL, 0x2020702020A04000ULL, 0x000011111111E100ULL, 0x0000111111A04000ULL, 0x000011115151A000ULL, 0x000011A040A01100ULL, 0x0000111111E101E0ULL, 0x0000F1804020F100ULL, 0xC02020102020C000ULL, 0x4040404040404000ULL, 0x6080800180806000ULL, 0x0000205180000000ULL, 0x51A051A051A05100ULL, 
0x000180406090F100ULL, 0xF10011A040A01100ULL, 0x00F111A0A0400000ULL, 0xC140404050604000ULL, 0x8041404040502000ULL, 0xF12140804021F100ULL, 0x3070F0F1F0703000ULL, 0x0000F1A0A0A0A000ULL, 0x204080E11111E000ULL, 0x01804020F100F100ULL, 0x10204080F100F100ULL, 0x0080F140F1200000ULL, 0x0000006190906100ULL, 0x004080F180400000ULL, 0x004020F120400000ULL, 0x4040404051E04000ULL, 
0x40E0514040404000ULL, 0x0000215180808000ULL, 0x402040E090906000ULL, 0x0000E010F010E000ULL, 0x0000A05141410101ULL, 0x609090F090906000ULL, 0x0010102040A01100ULL, 0x00C02121E0202010ULL, 0x0000E19090906000ULL, 0x0000E15040418000ULL, 0x000090115151A000ULL, 0x000040A011F10000ULL, 0xF1A0A0A0A0A0A000ULL, 0xE011111111A0B100ULL, 0x0000E0E0E0000000ULL, 0x0000A05151A00000ULL, 
0xC02170207021C000ULL, 0x4000404040404000ULL, 0x0040E15050E14000ULL, 0xC02120702020F100ULL, 0x11E0111111E01100ULL, 0x1111A0F140F14000ULL, 0x4040400040404000ULL, 0xC020E011E0806000ULL, 0xA000000000000000ULL, 0xE01171317111E000ULL, 0x6080E09060F00000ULL, 0x0041A050A0410000ULL, 0x000000F080000000ULL, 0x000000F000000000ULL, 0xE0117171B111E000ULL, 0xF100000000000000ULL, 
0xE0A0E00000000000ULL, 0x004040F14040F100ULL, 0xE080E020E0000000ULL, 0xE080E080E0000000ULL, 0x8040000000000000ULL, 0x0000009090907110ULL, 0xE171716141416100ULL, 0x0000006060000000ULL, 0x0000000000408060ULL, 0x604040E000000000ULL, 0xE01111E000F10000ULL, 0x0050A041A0500000ULL, 0x1090502051C10100ULL, 0x109050A111808100ULL, 0x3021B06071C10100ULL, 0x400040201011E000ULL, 
0x2040E011F1111100ULL, 0x8040E011F1111100ULL, 0x40A0E011F1111100ULL, 0xA050E011F1111100ULL, 0xA000E011F1111100ULL, 0xE0A0E011F1111100ULL, 0xA15050F15050D100ULL, 0xE011101011E08060ULL, 0x2040F110F010F100ULL, 0x8040F110F010F100ULL, 0x40A0F110F010F100ULL, 0xA000F110F010F100ULL, 0x2040E0404040E000ULL, 0x8040E0404040E000ULL, 0x40A0E0404040E000ULL, 0xA000E0404040E000ULL, 
0x60A0217121A06000ULL, 0x41A0113151911100ULL, 0x2040E0111111E000ULL, 0x8040E0111111E000ULL, 0x40A0E0111111E000ULL, 0xA050E0111111E000ULL, 0xA000E0111111E000ULL, 0x0011A040A0110000ULL, 0x01E0915131E01000ULL, 0x204011111111E000ULL, 0x804011111111E000ULL, 0x40A000111111E000ULL, 0xA00011111111E000ULL, 0x804011A040404000ULL, 0x7020E021E0207000ULL, 0xE011F01111F01010ULL, 
0x2040E001E111E100ULL, 0x8040E001E111E100ULL, 0x40A0E001E111E100ULL, 0xA050E001E111E100ULL, 0xA000E001E111E100ULL, 0xE0A0E001E111E100ULL, 0x0000B141F150F100ULL, 0x0000E11010E18060ULL, 0x2040E011F110E000ULL, 0x8040E011F110E000ULL, 0x40A0E011F110E000ULL, 0xA000E011F110E000ULL, 0x204000604040E000ULL, 0x804000604040E000ULL, 0x40A000604040E000ULL, 0xA00000604040E000ULL, 
0x80C180E090906000ULL, 0x41A000F011111100ULL, 0x204000E01111E000ULL, 0x804000E01111E000ULL, 0x40A000E01111E000ULL, 0x41A000E01111E000ULL, 0xA00000E01111E000ULL, 0x004000F100400000ULL, 0x000061905121D000ULL, 0x204000111111E100ULL, 0x804000111111E100ULL, 0x40A000111111E100ULL, 0xA00000111111E100ULL, 0x8040001111E101E0ULL, 0x0010709090701010ULL, 0xA000001111E101E0ULL };
void CArduViseur::text(int32_t x, int32_t y, int32_t w, char const *t, TPixel fg, TPixel bg)
{
    TPixel *c= fb+x+(y+7)*screenw;
    while (*t!=0 && w>=6)
    {
        uint64_t ca= font8[uint8_t(*t++)];
        TPixel *c2= c; c+= 6; w-= 6;
        for (int32_t y=8; --y>=0;)
        {
            *c2++= (ca&0x10)?fg:bg; *c2++= (ca&0x20)?fg:bg; *c2++= (ca&0x40)?fg:bg; *c2++= (ca&0x80)?fg:bg;
            *c2++= (ca&0x1)?fg:bg; *c2= (ca&0x2)?fg:bg;
            c2-= screenw+5; ca>>= 8;
        }
    }
    c-= 7*screenw;
    for (int32_t y=8; --y>=0;) { for (int32_t x=w; --x>=0;) *c++= bg; c+= screenw-w; }
}
void CArduViseur::text(int32_t x, int32_t y, int32_t w, char const *t, TPixel fg)
{
    TPixel *c= fb+x+(y+7)*screenw;
    while (*t!=0 && w>=6)
    {
        uint64_t ca= font8[uint8_t(*t++)];
        TPixel *c2= c; c+= 6; w-= 6;
        for (int32_t y=8; --y>=0;)
        {
            if (ca&0x10) c2[0]= fg; if (ca&0x20) c2[1]= fg; if (ca&0x40) c2[2]= fg; if (ca&0x80) c2[3]= fg;
            if (ca&0x1) c2[4]= fg; if (ca&0x2) c2[5]= fg;
            c2-= screenw; ca>>= 8;
        }
    }
}
void CArduViseur::textClip(int32_t x, int32_t y, char const *t, TPixel fg)
{
    TPixel *c= fb+x+(y+7)*screenw;
    while (*t!=0)
    {
        TPixel *c2= c; c+= 6;
        uint64_t ca= font8[uint8_t(*t++)];
        if (x>=clipX) return; 
        if (x+6>=clipx) 
            for (int32_t Y=8; --Y>=0;)
            {
                if (y+Y>=clipy && y+Y<clipY)
                {
                  if (ca&0x10) if (x>=clipx && x<clipX) c2[0]= fg; 
                  if (ca&0x20) if (x+1>=clipx && x+1<clipX) c2[1]= fg; 
                  if (ca&0x40) if (x+2>=clipx && x+2<clipX) c2[2]= fg; 
                  if (ca&0x80) if (x+3>=clipx && x+3<clipX) c2[3]= fg;
                  if (ca&0x1)  if (x+4>=clipx && x+4<clipX) c2[4]= fg;
                }
                c2-= screenw; ca>>= 8;
            }
        x+= 6;
    }
}

void CArduViseur::penEvent() // generates clics and drags from HW pen readings...
{
    bool down= readTP(penx, peny);
    if (!down) 
    { 
        if (penDown) click(-1, -1);
        penDown= false, penDrag= false; 
        return; 
    } 
    touchToPoint(penx, peny);
    if (!penDown) { penInitx= penx; penInity= peny; click(penx, peny); penDown= true; penDrag= false; }
    int32_t dx= penx-penInitx, dy= peny-penInity;
    if (penDrag) drag(penx, peny, dx, dy); 
    else {
        int32_t delta= dx*dx+dy*dy;
        if (delta>10*10) { penDrag= true; drag(penx, peny, dx, dy); }
    }
}

static int todeg(float t) { return int(t*180.0f/MPI); }
static float radnorm(float a) { while (a<0.0f) a+= MPI*2.0f; while (a>=MPI*2.0f) a-= MPI*2.0f; return a; }

static int const buttonHeight= 25;

static bool clickButton(int b, int32_t x, int32_t y)
{
    return y>=b*buttonHeight && y<b*buttonHeight+buttonHeight && x>=0 && x<buttonHeight;
}

static int const buttonFieldMore= 1;
static int const buttonFieldLess= 2;
static int const buttonDisplayMode= 5;
static int const buttonFind= 6;
static int const screenShot= 8;
#define findModeParams 40, 20, 320-50, 200
static bool gotoOnRelease;
static float gotodec, gotora;

void CArduViseur::click(int32_t x, int32_t y)
{
    if (tpCal.initStage<3) return clickCalibration(x, y);
    //debug("click%d %d", int(x), int(y)); if (x>=0) pixel(x, y, 0xffff); return;
    if (x==-1)
    {
        if (penDrag) return;
        if (gotoOnRelease && findMode!=0)
        {
            setSkydec(gotodec), setSkyra(gotora), findMode= 0;
            return;
        }
        displayText[0]= 0;
        TDisplayObjType *d= findDspObj(penx, peny);
        if (d==nullptr) return;
        char t1[50]="", t2[50]="";
        if (d->type==0)
        {  
            TStarMore const *sm= stars[d->index].moreInfo(); if (sm==nullptr) return;
            sm->getName(t1);
            strcpy(t2, bayerGreek[sm->bayerGreek()]); strcat(t2, bayerConst[sm->bayerConst()]);
            sprintf(displayText, "%s%s %ldly mag:%ld", t1, t2, sm->dist(), sm->absMag());
            return;
        }
        TSkyCatalog const *sk= nullptr; char const *name= nullptr; char prefix;
        if (d->type==1) { sk= &messier[d->index]; name= sk->getString(messierStrings); prefix= 'M'; }
        if (d->type==2) { sk= &caldwell[d->index]; name= sk->getString(cadwellStrings); prefix= 'C'; }
        int ip; if (int fp= sk->getSze(ip)) { itoa(ip, t1, 10); strcat(t1, "."); itoa(fp, t1+strlen(t1),10); } else itoa(ip, t1, 10);
        if (name==nullptr)
            sprintf(displayText, "%c%ld %ldly mag:%ld %smin", prefix, d->index, sk->getDst(), sk->mag, t1);
        else
            sprintf(displayText, "%c%ld %s %ldly mag:%ld %smin", prefix, d->index, name, sk->getDst(), sk->mag, t1);
        return;
    }
    skydownra= 1000.0f;
    if (clickButton(buttonFieldMore, x, y)) return keyPress(0);
    if (clickButton(buttonFieldLess, x, y)) return keyPress(1);
    if (clickButton(buttonDisplayMode, x, y)) return keyPress(2);
    if (clickButton(buttonFind, x, y)) return keyPress(3);
    if (clickButton(screenShot, x, y)) return keyPress(10);
    XYToraDec(x, y, skydownra, skydowndec);
    if (findMode!=0)
    {
        if (inCenterRectText(x, y, findModeParams, 0)) { findMode= 1; generateTextAndPos(); return; }
        if (inCenterRectText(x, y, findModeParams, 1)) { findMode= 2; generateTextAndPos(); return; } 
        if (inCenterRectText(x, y, findModeParams, 2)) { findMode= 3; generateTextAndPos(); return; }
        if (inCenterRectText(x, y, findModeParams, 3)) { findMode= 4; generateTextAndPos(); return; }
        for (int i=0; i<drawFindTextPosNb; i++)
            if (x>=drawFindTextPos[i].x && x<drawFindTextPos[i].X && y>=drawFindTextPos[i].y && y<drawFindTextPos[i].Y)
            {
                gotodec= drawFindTextPos[i].dec, gotora= drawFindTextPos[i].ra, gotoOnRelease= true;
                return;
            }
    }
}
void CArduViseur::keyPress(int keyId)
{
    if (keyId==0) { if (viewAnglei>0) setViewAnglei(viewAnglei-1); return; } 
    if (keyId==1) { if (viewAnglei<7) setViewAnglei(viewAnglei+1); return; }
    if (keyId==2) { displayMode++; if (displayMode==5) displayMode= 0; return; }
    if (keyId==3) { if (findMode!=0) findMode= 0; else findMode= 1; generateTextAndPos(); return; }
    if (keyId==10) 
    { 
      uint8_t *d= camImg;
      for (int y=0; y<camH; y++)
      {
        char t[1000];
        for (int x=0; x<camW; x++)
        {
          char const toh[]="0123456789ABCDEF";
          t[x*5]='0';t[x*5+1]='x';t[x*5+2]=toh[d[0]>>4];t[x*5+3]=toh[d[0]&15]; t[x*5+4]=','; d++;
        }
        t[camW*5]=0; debug(t);
      }
      return; 
    }
}

void CArduViseur::drag(int32_t px, int32_t py, int32_t dx, int32_t dy)
{
    // debug("drag %d %d", int(px), int(py)); pixel(px, py, 0xffff); return;
    // if in a mode with sky, move the sky
    // if in a mode with a slider, then handle that...
    if (findMode!=0)
    {
        penInitx= px; penInity= py;
        if (findMode==1) { findModeM-= dx; if (findModeM<0) findModeM= 0; if (findModeM>findModeMax) findModeM= findModeMax; }
        if (findMode==2) { findModeC-= dx; if (findModeC<0) findModeC= 0; if (findModeC>findModeMax) findModeC= findModeMax; }
        if (findMode==3) { findModeStarConst-= dx; if (findModeStarConst<0) findModeStarConst= 0; if (findModeStarConst>findModeMax) findModeStarConst= findModeMax; }
        if (findMode==4) { findModeStarConstAlpha-= dx; if (findModeStarConstAlpha<0) findModeStarConstAlpha= 0; if (findModeStarConstAlpha>findModeMax) findModeStarConstAlpha= findModeMax; }
        return;
    }
    if (skydownra!=1000.0f)
    {
        SkyForXYRaDec(px, py, skydownra, skydowndec);
    }
}

static void drawLabel(CArduViseur *v, int32_t b, char const *t)
{
    int w= int(strlen(t)*6);
    v->text((30-w)/2, b*buttonHeight+15-4, w, t, 0);
}
static void drawButton(CArduViseur *v, int32_t b, char const *t)
{
    v->rect(0, b*buttonHeight, 30, buttonHeight, 0);
    drawLabel(v, b, t);
}

void firstrun()
{
    //psi=arccos(sinDec1*sinDec2+cosDec1*cosDec2*cos(Ra1−Ra2)).
    // list all stars of mag>10 within 3deg of each other
    int cnt= 0, cntMag10= 0, discard= 0, maxDst= 0;
    float const sensibility= 3.0f;
    float cos3= cosf(sensibility*degToRad);
    int iM= 0;
    for (int i=0; i<nbStars-1; i++)
    {
        if (stars[i].mag()>=8.0) continue;
        cntMag10++;
        float sd= stars[i].dec(), sra= stars[i].ra();
        while (iM<nbStars && abs(sd-stars[iM].dec())<=sensibility*degToRad) iM++;
        for (int j=i+1; j<iM; j++)
        {
            if (stars[j].mag()>=8.0) continue;
            float sd2= stars[j].dec(), sra2= stars[j].ra();
            if (acosf(sinf(sd)*sinf(sd2)+cosf(sd)*cosf(sd2)*cosf(sra-sra2))>sensibility*degToRad) { discard++; continue; }
            cnt++;
            if (j-i>maxDst) maxDst= j-i;
        }
    }
    debug("%d stars of mag>8. %d couple within sensitivity. %d discarded, maxDst:%d", cntMag10, cnt, discard, maxDst);
    // for 3deg: 41135 stars of mag>8. 2777460 couple within sensitivity. 30884460 discarded
}

void CArduViseur::draw()
{
//    static bool done=false; if (!done) { firstrun(); done= true; }
    if (tpCal.initStage<3) return drawCalibration();
    // draw buttons
    uint32_t x= 30; // button width
    if (displayMode==dmSkyCameraInsert || displayMode==dmSky) drawSky(x, 0, screenw-x, screenh); 
    if (displayMode==dmSkyCameraInsert) drawCamera(screenw-camInsertW, screenh-camInsertH, true);
    if (displayMode==dmCameraSkyInsert || displayMode==dmCamera) drawCamera(x, 0, false);
    if (displayMode==dmCameraSkyInsert) drawSky(screenw-skyInsertW, screenh-skyInsertH, skyInsertW, skyInsertH);
    if (displayMode==dmSkyCamera) { drawSky(x, 0, (screenw-x)/2, screenh); drawCamera(x+(screenw-x)/2, 0, false); }
    fillRect(0, 0, x, screenh, 0xffff);
    drawButton(this, 1, "F+");
    drawButton(this, 2, "F-");
    char const angles[8][4]= { "50\xb0", "40\xb0", "30\xb0", "20\xb0", "10\xb0", "5\xb0", "2\xb0", "1\xb0" }; // 0=50 1=40 2=30 3=20 4=10 5=5 6=2 7=1
    drawLabel(this, 0, angles[viewAnglei]);
    char t[10]; int d= todeg(skydec); if (d>180) d= -(360-d);
    drawLabel(this, 3, itoa(d, t, 10));
    drawLabel(this, 4, itoa(todeg(skyra), t, 10));
    text(0, screenh-8, screenw, displayText, 63<<5);
    drawButton(this, buttonDisplayMode, "Mde");
    drawButton(this, buttonFind, "Fnd");
    drawButton(this, screenShot, "cap");
    if (findMode) drawFind(findModeParams);
}

static int32_t const calP[6]= { 10, 10, 160, 230, 310, 60 };

void CArduViseur::drawCalibration()
{
    fillRect(0, 0, screenw, screenh, 0);
    fillRect(calP[2*tpCal.initStage]-3, calP[2*tpCal.initStage+1]-3, 7, 7, 0xffff);
    textCenter(screenw/2, screenh/2, "Please click on point", 0xffff);
}
void CArduViseur::clickCalibration(int32_t x, int32_t y)
{
    if (x<0 || y<0) return;
    calT[tpCal.initStage*2]= x;
    calT[tpCal.initStage*2+1]= y;
    if (++tpCal.initStage<3) return;
    touchToPointInit(calP, calT);
    fillRect(0, 0, 320, 240, 0);
}

void CArduViseur::drawCamera(int32_t x, int32_t y, bool zoom)
{
    if (camImg==nullptr) return;
    //blit(x, y, camImg, camW, camH);
    uint8_t *b= camImg;
    TPixel *d= fb+x+y*screenw;
    if (!zoom) 
      for (int32_t Y=0; Y<camH; Y++)
      {
          int32_t ty= y+Y; if (ty<clipy || ty>=clipY) continue;
          for (int32_t X=0; X<camW; X++)
          {
              int32_t tx= x+X; b++; 
              if (tx<clipx || tx>=clipX) continue;
              uint32_t c= b[-1]; d[X]= ((c&62)<<10)|(c<<5)|((c>>1)&31);
          }
          d+= screenw;
      }
    else
      for (int32_t Y=0; Y<camH/2; Y++)
      {
          int32_t ty= y+Y; if (ty<clipy || ty>=clipY) continue;
          for (int32_t X=0; X<camW/2; X++)
          {
              int32_t tx= x+X; b+= 2;
              if (tx<clipx || tx>=clipX) continue;
              uint32_t c= (b[-2]+b[-1]+b[camW-1]+b[camW-2])>>2;
              d[X]= ((c&62)<<10)|(c<<5)|((c>>1)&31);
          }
          b+= camW;
          d+= screenw;
      }
}

static uint8_t const starPatternsV[]={
      31,
      0,9,0,9,31,9,0,9,0,
      0,16,0,16,31,16,0,16,0,
      0,24,0,24,31,24,0,24,0,
      0,31,0,31,31,31,0,31,0,
      9,31,9,31,31,31,9,31,9,
      16,31,16,31,31,31,16,31,16,
      24,31,24,31,31,31,24,31,24,
      31,31,31,31,31,31,31,31,31,
      0,3,3,3,0,3,31,31,31,3,3,31,31,31,3,3,31,31,31,3,0,3,3,3,0,
      0,6,6,6,0,6,31,31,31,6,6,31,31,31,6,6,31,31,31,6,0,6,6,6,0,
      0,8,8,8,0,8,31,31,31,8,8,31,31,31,8,8,31,31,31,8,0,8,8,8,0,
      0,11,11,11,0,11,31,31,31,11,11,31,31,31,11,11,31,31,31,11,0,11,11,11,0,
      0,14,14,14,0,14,31,31,31,14,14,31,31,31,14,14,31,31,31,14,0,14,14,14,0,
      0,16,16,16,0,16,31,31,31,16,16,31,31,31,16,16,31,31,31,16,0,16,16,16,0,
      0,19,19,19,0,19,31,31,31,19,19,31,31,31,19,19,31,31,31,19,0,19,19,19,0,
      0,21,21,21,0,21,31,31,31,21,21,31,31,31,21,21,31,31,31,21,0,21,21,21,0,
      0,24,24,24,0,24,31,31,31,24,24,31,31,31,24,24,31,31,31,24,0,24,24,24,0,
      0,26,26,26,0,26,31,31,31,26,26,31,31,31,26,26,31,31,31,26,0,26,26,26,0,
      0,29,29,29,0,29,31,31,31,29,29,31,31,31,29,29,31,31,31,29,0,29,29,29,0,
      0,31,31,31,0,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,0,31,31,31,0,
      0,0,3,3,3,0,0,0,3,31,31,31,3,0,3,31,31,31,31,31,3,3,31,31,31,31,31,3,3,31,31,31,31,31,3,0,3,31,31,31,3,0,0,0,3,3,3,0,0,
      0,0,4,4,4,0,0,0,4,31,31,31,4,0,4,31,31,31,31,31,4,4,31,31,31,31,31,4,4,31,31,31,31,31,4,0,4,31,31,31,4,0,0,0,4,4,4,0,0,
      0,0,6,6,6,0,0,0,6,31,31,31,6,0,6,31,31,31,31,31,6,6,31,31,31,31,31,6,6,31,31,31,31,31,6,0,6,31,31,31,6,0,0,0,6,6,6,0,0,
      0,0,8,8,8,0,0,0,8,31,31,31,8,0,8,31,31,31,31,31,8,8,31,31,31,31,31,8,8,31,31,31,31,31,8,0,8,31,31,31,8,0,0,0,8,8,8,0,0,
      0,0,10,10,10,0,0,0,10,31,31,31,10,0,10,31,31,31,31,31,10,10,31,31,31,31,31,10,10,31,31,31,31,31,10,0,10,31,31,31,10,0,0,0,10,10,10,0,0,
      0,0,12,12,12,0,0,0,12,31,31,31,12,0,12,31,31,31,31,31,12,12,31,31,31,31,31,12,12,31,31,31,31,31,12,0,12,31,31,31,12,0,0,0,12,12,12,0,0,
      0,0,14,14,14,0,0,0,14,31,31,31,14,0,14,31,31,31,31,31,14,14,31,31,31,31,31,14,14,31,31,31,31,31,14,0,14,31,31,31,14,0,0,0,14,14,14,0,0,
      0,0,16,16,16,0,0,0,16,31,31,31,16,0,16,31,31,31,31,31,16,16,31,31,31,31,31,16,16,31,31,31,31,31,16,0,16,31,31,31,16,0,0,0,16,16,16,0,0,
      0,0,18,18,18,0,0,0,18,31,31,31,18,0,18,31,31,31,31,31,18,18,31,31,31,31,31,18,18,31,31,31,31,31,18,0,18,31,31,31,18,0,0,0,18,18,18,0,0,
      0,0,20,20,20,0,0,0,20,31,31,31,20,0,20,31,31,31,31,31,20,20,31,31,31,31,31,20,20,31,31,31,31,31,20,0,20,31,31,31,20,0,0,0,20,20,20,0,0,
      0,0,22,22,22,0,0,0,22,31,31,31,22,0,22,31,31,31,31,31,22,22,31,31,31,31,31,22,22,31,31,31,31,31,22,0,22,31,31,31,22,0,0,0,22,22,22,0,0,
};
static struct { uint8_t const *p, size; } const starPatterns[32]={
      { starPatternsV+863, 7},
      { starPatternsV+814, 7},
      { starPatternsV+765, 7},
      { starPatternsV+716, 7},
      { starPatternsV+667, 7},
      { starPatternsV+618, 7},
      { starPatternsV+569, 7},
      { starPatternsV+520, 7},
      { starPatternsV+471, 7},
      { starPatternsV+422, 7},
      { starPatternsV+373, 7},
      { starPatternsV+348, 5},
      { starPatternsV+323, 5},
      { starPatternsV+298, 5},
      { starPatternsV+273, 5},
      { starPatternsV+248, 5},
      { starPatternsV+223, 5},
      { starPatternsV+198, 5},
      { starPatternsV+173, 5},
      { starPatternsV+148, 5},
      { starPatternsV+123, 5},
      { starPatternsV+98, 5},
      { starPatternsV+73, 5},
      { starPatternsV+64, 3},
      { starPatternsV+55, 3},
      { starPatternsV+46, 3},
      { starPatternsV+37, 3},
      { starPatternsV+28, 3},
      { starPatternsV+19, 3},
      { starPatternsV+10, 3},
      { starPatternsV+1, 3},
      { starPatternsV+0, 1},
};

bool CArduViseur::raDecToXY(float ra, float dec, float &x, float &y)
{
    // to cartesian with rotation by a and d...
    float fx= lastSky.cosSkyd*cosf(ra-skyra)*cosf(dec) + lastSky.sinSkyd*sinf(dec);
    if (fx<=0.1f) return false;
    float fy= sinf(ra-skyra)*cosf(dec) / fx;
    float fz= (-lastSky.sinSkyd*cosf(ra-skyra)*cosf(dec) + lastSky.cosSkyd*sinf(dec)) / fx;
    // x/y/z are now on the radius 1 sphere...
    // rotation of the screen/camera
    float rx= fy*lastSky.cosSkyrot-fz*lastSky.sinSkyrot, ry= fy*lastSky.sinSkyrot+fz*lastSky.cosSkyrot;
    // projection (scaling really)...
    rx*= lastSky.scale, ry*= lastSky.scale;
    if (abs(rx)>1024.0f || abs(ry)>1024.0f) return false; // clipping
    // and finally, shift to position
    x= lastSky.x+lastSky.w/2+rx, y= lastSky.y+lastSky.h/2-ry;
    return true;
}

static float const fullSkyAngles[8]= { dtr(50), dtr(40), dtr(30), dtr(20), dtr(10), dtr(5), dtr(2), dtr(1) };

bool CArduViseur::XYToraDec(int32_t x, int32_t y, float &ra, float &dec)
{ // I tried using reverse math, but the to cartesian + rotation in one go can not/is hard to inverse... so, doing a 2 equation solve using raDecToXY. should be fast enough for governement purpose!
    ra= skyra, dec= skydec; float x1, y1; raDecToXY(ra, dec, x1, y1); // get default return...
    float step= fullSkyAngles[viewAnglei]/100.0f;
    float d=100.0f;
    for (int i=0; i<200; i++)
    {
        if (d<6.0f) break; // close enough (2.5pixels)
        float ra2= radnorm(ra+step), dec2= radnorm(dec);      float x2, y2; raDecToXY(ra2, dec2, x2, y2); float d2= abs(x2-x)*abs(x2-x)+abs(y2-y)*abs(y2-y);
        float ra3= radnorm(ra-step), dec3= radnorm(dec);      float x3, y3; raDecToXY(ra3, dec3, x3, y3); float d3= abs(x3-x)*abs(x3-x)+abs(y3-y)*abs(y3-y);
        float ra4= radnorm(ra),      dec4= radnorm(dec+step); float x4, y4; raDecToXY(ra4, dec4, x4, y4); float d4= abs(x4-x)*abs(x4-x)+abs(y4-y)*abs(y4-y);
        float ra5= radnorm(ra),      dec5= radnorm(dec-step); float x5, y5; raDecToXY(ra5, dec5, x5, y5); float d5= abs(x5-x)*abs(x5-x)+abs(y5-y)*abs(y5-y);
        if (d2<=d3 && d2<=d4 && d2<=d5) { d= d2; ra= ra2, dec= dec2; x1= x2, y1= y2; continue; }
        else if (d3<=d2 && d3<=d4 && d3<=d5) { d= d3; ra= ra3, dec= dec3; x1= x3, y1= y3; continue; }
        else if (d4<=d2 && d4<=d3 && d4<=d5) { d= d4; ra= ra4, dec= dec4; x1= x4, y1= y4; continue; }
        else if (d5<=d2 && d5<=d3 && d5<=d4) { d= d5; ra= ra5, dec= dec5; x1= x5, y1= y5; continue; }
    }
    //qDebug() << "reverse lookup at x/y" << x << y << "found ra/dec" << todeg(ra) << todeg(dec) << "for x/y" << x1 << y1 << "d=" << d;
    return true;
}
// What sky point do we need to aim to so that the given ra/dec point to a given pixel..
bool CArduViseur::SkyForXYRaDec(int32_t x, int32_t y, float ra, float dec)
{
    int32_t x1, y1; if (!raDecToXY(ra, dec, x1, y1)) return false;
    if (abs(x1-x)*abs(x1-x)+abs(y1-y)*abs(y1-y)<6.0f) return true; // good enough!
    float step= fullSkyAngles[viewAnglei]/100.0f;
    float d=100.0f;
    for (int i=0; i<200; i++)
    {
        if (d<6.0f) break; // close enough (2.5pixels)
        float tskyra= skyra, tskydec= skydec;
        skyra= tskyra+step, skydec= tskydec;      lastSky.sinSkyd= sinf(skydec), lastSky.cosSkyd= cosf(skydec); float x2, y2; raDecToXY(ra, dec, x2, y2); float d2= abs(x2-x)*abs(x2-x)+abs(y2-y)*abs(y2-y);
        skyra= tskyra-step, skydec= tskydec;      lastSky.sinSkyd= sinf(skydec), lastSky.cosSkyd= cosf(skydec); float x3, y3; raDecToXY(ra, dec, x3, y3); float d3= abs(x3-x)*abs(x3-x)+abs(y3-y)*abs(y3-y);
        skyra= tskyra,      skydec= tskydec+step; lastSky.sinSkyd= sinf(skydec), lastSky.cosSkyd= cosf(skydec); float x4, y4; raDecToXY(ra, dec, x4, y4); float d4= abs(x4-x)*abs(x4-x)+abs(y4-y)*abs(y4-y);
        skyra= tskyra,      skydec= tskydec-step; lastSky.sinSkyd= sinf(skydec), lastSky.cosSkyd= cosf(skydec); float x5, y5; raDecToXY(ra, dec, x5, y5); float d5= abs(x5-x)*abs(x5-x)+abs(y5-y)*abs(y5-y);
        if (d2<=d3 && d2<=d4 && d2<=d5)      { d= d2; skyra= tskyra+step, skydec= tskydec;      continue; }
        else if (d3<=d2 && d3<=d4 && d3<=d5) { d= d3; skyra= tskyra-step, skydec= tskydec;      continue; }
        else if (d4<=d2 && d4<=d3 && d4<=d5) { d= d4; skyra= tskyra,      skydec= tskydec+step; continue; }
        else if (d5<=d2 && d5<=d3 && d5<=d4) { d= d5; skyra= tskyra,      skydec= tskydec-step; continue; }
    }
    setSkyra(radnorm(skyra)); setSkydec(radnorm(skydec)); lastSky.sinSkyd= sinf(skydec), lastSky.cosSkyd= cosf(skydec);
    //raDecToXY(ra, dec, x1, y1); qDebug() << "reverse lookup2 want ra/dec at x/y" << todeg(ra) << todeg(dec) << x << y << "found skyra/dec" << todeg(skyra) << todeg(skydec) << "for x/y d" << x1 << y1 << d;
    return true;
}

CArduViseur::TDisplayObjType *CArduViseur::findDspObj(int32_t x, int32_t y)
{
    uint32_t bd= 0xffffffff; TDisplayObjType *b= nullptr;
    for (int i=nbDisplayObjs; --i>=0;)
    {
        uint32_t d= (x-displayObjs[i].x)*(x-displayObjs[i].x)+(y-displayObjs[i].y)*(y-displayObjs[i].y);
        if (d<bd) { b= &displayObjs[i]; bd= d; }
    }
    if (bd<25) return b; else return nullptr;
}

void CArduViseur::drawSky(int32_t x, int32_t y, int32_t w, int32_t h)
{
    nbDisplayObjs= 0;
    float viewAngler= fullSkyAngles[viewAnglei];
   
    lastSky.x= x, lastSky.y= y, lastSky.w= w, lastSky.h= h; 
    lastSky.scale= w/(2.0f*tanf(viewAngler/2.0f)); lastSky.sinSkyd= sinf(skydec), lastSky.cosSkyd= cosf(skydec);
    lastSky.cosSkyrot= cosf(skyrot), lastSky.sinSkyrot= sinf(skyrot);

    TStarLink const *sl= starLink;

    int32_t clip[4]; saveClip(clip, x, y, x+w, y+h);
    fillRect(x, y, w, h, 0);
    int drawnStars= 0;
    for (uint32_t i=0; i<nbStars; i++)
    {
        while (uint32_t(i)>sl->first) sl++;
        int32_t mag= stars[i].imag;
        float dec= stars[i].dec()-MPI/2.0f, ra= -stars[i].ra();
        // mag=10 means mag 3. mag=12->mag=4 and then 2 for 1...
        if (!stars[i].hasMoreInfo && i!=sl->first)
        {
            if (mag>limMag) continue;
            // add dec based filtering!
        }
        float fsx, fsy; if (!raDecToXY(ra, dec, fsx, fsy)) continue; int32_t sx= int32_t(fsx), sy= int32_t(fsy); 
        if (lightUp(sx-starPatterns[mag].size/2, sy-starPatterns[mag].size/2, starPatterns[mag].p, starPatterns[mag].size, starPatterns[mag].size, 7))
        {
            drawnStars++;
            if (stars[i].hasMoreInfo) addDspObj(sx, sy, i, 0);
        }
        while (sl->first==i)
        {
            uint32_t s2= i+sl++->delta; float dec2= stars[s2].dec()-MPI/2.0f, ra2= -stars[s2].ra(); float sx2, sy2; if (!raDecToXY(ra2, dec2, sx2, sy2)) continue;
            line(fsx, fsy, sx2, sy2, 0xffff);
        }
    }
    if (findLimMag){ if (drawnStars<50 && limMag<31) limMag++; else findLimMag= false; }
    // Draw messier
    for (int32_t i=0; i<110; i++)
    {
        float dec= messier[i].getdec(), ra= -messier[i].getra();
        int32_t sx, sy; if (!raDecToXY(ra, dec, sx, sy)) continue;
        int mag= 0;
        if (lightUp(sx-starPatterns[mag].size/2, sy-starPatterns[mag].size/2, starPatterns[mag].p, starPatterns[mag].size, starPatterns[mag].size, 3))
          addDspObj(sx, sy, i, 1);
    }
    // Draw cadwell
    for (int32_t i=0; i<109; i++)
    {
        float dec= caldwell[i].getdec(), ra= -caldwell[i].getra();
        int32_t sx, sy; if (!raDecToXY(ra, dec, sx, sy)) continue;
        int mag= 0;
        if (lightUp(sx-starPatterns[mag].size/2, sy-starPatterns[mag].size/2, starPatterns[mag].p, starPatterns[mag].size, starPatterns[mag].size, 2))
          addDspObj(sx, sy, i, 2);
    }

    float const angleStepsC[8]={dtr(5), dtr(5), dtr(5), dtr(5), dtr(2), dtr(1), dtr(0.5), dtr(0.1)};
    float angleStepsd= angleStepsC[viewAnglei];
    // depending on FOV and dec, choose the proper ra stepping > angleStepsd...
    // if dec close to pi/2, then we can use the same step as for dec...
    // as it gets higher, and closer to zenith (dec=0 or PI), then we need to decreese the number of radians...
    // 2*tan(fov/2) is width of fov
    // 2*tan(rasteps/2)*sin(dec) is width of 1 fuseau
    // we want at most N fuseaux on the screen, so if f*N>fov, make fuseau bigger... (avoids /0)
    float const angleStepsC2[]={dtr(0.1), dtr(0.5), dtr(1), dtr(2), dtr(5), dtr(10), dtr(20), dtr(30)};
    float angleStepsra;
    for (int i=0; i<8; i++)
    {
        angleStepsra= angleStepsC2[i]; if (angleStepsra<angleStepsd) continue;
        float fov= abs(2.0f*tanf(viewAngler/2.0f)), fuseau= abs(2.0f*tanf(angleStepsra/2.0f)*cosf(skydec));
        if (fuseau*10.0f>fov) break;
    }
    float mul= 1.0f;
    float ras= skyra-viewAngler*mul-fmodf(skyra-viewAngler*mul, angleStepsra);
    float decs= skydec-viewAngler*mul-fmodf(skydec-viewAngler*mul, angleStepsd);
    for (int i=0; i<20; i++)
    {
        if (i*angleStepsra>MPI) break;
        float x1, y1; bool b1= raDecToXY(ras+i*angleStepsra, decs, x1, y1);
        for (int j=1; j<20; j++)
        {
            float x2, y2; bool b2= raDecToXY(ras+i*angleStepsra, decs+j*angleStepsd, x2, y2);
            if (b1&&b2) line(x1, y1, x2, y2, 31<<(5+6)); x1= x2, y1= y2; b1= b2;
        }
    }
    for (int i=0; i<20; i++)
    {
        float x1, y1; bool b1= raDecToXY(ras, i*angleStepsd+decs, x1, y1);
        for (int j=1; j<20; j++)
        {
            if (j*angleStepsra>MPI) break;
            float x2, y2; bool b2= raDecToXY(ras+j*angleStepsra, decs+i*angleStepsd, x2, y2);
            if (b1&&b2) line(x1, y1, x2, y2, 31<<(5+6)); x1= x2, y1= y2; b1= b2;
        }
    }
    restoreClip(clip);
}

void CArduViseur::solve()
{
}

void CArduViseur::tick()
{
    if (tpCal.initStage>=3)
    {
      uint32_t now1= getNow();
      if (getCamera(camImg, camW, camH)) // if new frame availalbe... solve.
      {
        uint32_t now2= getNow(); lastCamera= now2-now1;
        solve();
        now1= getNow(); lastSolve= now1-now2; 
      }
    }
    penEvent();
    handleKeys();
    uint32_t now3= getNow();
    //if (track) { skyra= platea; skydec= plated; }
    draw(); // takes around 13ms on arduino...
    uint32_t now4= getNow();
    lastDraw= now4-now3; 
}

void CArduViseur::handleKeys()
{
    uint32_t b= getButtons();
    uint32_t newKeyDown= b&~lastKeyboard;
    lastKeyboard= b;
    if (newKeyDown==0) return;
    uint32_t now= getNow();
    for (int i=0; i<8; i++) 
        if ((newKeyDown&(1<<i))!=0)
            if (lastKeyDown!=i || now-lastKeyPressTime>100) 
            {
                keyPress(i);
                lastKeyDown=i; lastKeyPressTime= now;
            }
}

float calc_rad(float mag)
{
    if (mag >= 12) return .5;
    if (mag >= 10) return .5;
    if (mag >= 9 ) return .5;
    if (mag >= 8 ) return 1 ;
    if (mag >= 7 ) return 1 ;
    if (mag >= 6 ) return 2 ;
    if (mag >= 5 ) return 2 ;
    if (mag >= 4 ) return 3 ;
    if (mag >= 3 ) return 4 ;
    if (mag >= 2 ) return 5 ;
    if (mag >= 1 ) return 6 ;
    if (mag >= 0 ) return 7 ;
    if (mag >= -1) return 8 ;
    return 10;
}


////////////////////////////////////////////////////////
// calibration
// equation, we have a transform matrix with 6 coefs to pass from tp coordinates to screen coordinates
// we know 3 points, P1/2/3 and their corresponding TP values T1/2/3. cas solves this for us...
//    x=a*tx+b*ty+c
//    y=d*tx+e*ty+f
//
//     solve([P1x=a*T1x+b*T1y+c,
//            P1y=d*T1x+e*T1y+f,
//            P2x=a*T2x+b*T2y+c,
//            P2y=d*T2x+e*T2y+f,
//            P3x=a*T3x+b*T3y+c,
//            P3y=d*T3x+e*T3y+f],[a,b,c,d,e,f])
//
//
//        a=(P1x*T2y-P1x*T3y-P2x*T1y+P2x*T3y+P3x*T1y-P3x*T2y)                        /(T1x*T2y-T1x*T3y-T1y*T2x+T1y*T3x+T2x*T3y-T2y*T3x)
//        b=(-P1x*T2x+P1x*T3x+P2x*T1x-P2x*T3x-P3x*T1x+P3x*T2x)                       /(T1x*T2y-T1x*T3y-T1y*T2x+T1y*T3x+T2x*T3y-T2y*T3x)
//        c=(P1x*T2x*T3y-P1x*T2y*T3x-P2x*T1x*T3y+P2x*T1y*T3x+P3x*T1x*T2y-P3x*T1y*T2x)/(T1x*T2y-T1x*T3y-T1y*T2x+T1y*T3x+T2x*T3y-T2y*T3x)
//        d=(P1y*T2y-P1y*T3y-P2y*T1y+P2y*T3y+P3y*T1y-P3y*T2y)                        /(T1x*T2y-T1x*T3y-T1y*T2x+T1y*T3x+T2x*T3y-T2y*T3x)
//        e=(-P1y*T2x+P1y*T3x+P2y*T1x-P2y*T3x-P3y*T1x+P3y*T2x)                       /(T1x*T2y-T1x*T3y-T1y*T2x+T1y*T3x+T2x*T3y-T2y*T3x)
//        f=(P1y*T2x*T3y-P1y*T2y*T3x-P2y*T1x*T3y+P2y*T1y*T3x+P3y*T1x*T2y-P3y*T1y*T2x)/(T1x*T2y-T1x*T3y-T1y*T2x+T1y*T3x+T2x*T3y-T2y*T3x)
void CArduViseur::touchToPoint(int32_t &x, int32_t &y)
{
    int32_t tx= (tpCal.a*x+tpCal.b*y+tpCal.c)/256;
    int32_t ty= (tpCal.d*x+tpCal.e*y+tpCal.f)/256;
    x= tx; y= ty;
}
bool CArduViseur::touchToPointInit(int32_t const *P, int32_t const *T)
{
    float div= T[0]*T[3]-T[0]*T[5]-T[1]*T[2]+T[1]*T[4]+T[2]*T[5]-T[3]*T[4];
    if (div==0) return false; 
    tpCal.a=int32_t(256*float(P[0]*T[3]-P[0]*T[5]-P[2]*T[1]+P[2]*T[5]+P[4]*T[1]-P[4]*T[3])                        /div);
    tpCal.b=int32_t(256*float(-P[0]*T[2]+P[0]*T[4]+P[2]*T[0]-P[2]*T[4]-P[4]*T[0]+P[4]*T[2])                       /div);
    tpCal.c=int32_t(256*float(P[0]*T[2]*T[5]-P[0]*T[3]*T[4]-P[2]*T[0]*T[5]+P[2]*T[1]*T[4]+P[4]*T[0]*T[3]-P[4]*T[1]*T[2])/div);
    tpCal.d=int32_t(256*float(P[1]*T[3]-P[1]*T[5]-P[3]*T[1]+P[3]*T[5]+P[5]*T[1]-P[5]*T[3])                        /div);
    tpCal.e=int32_t(256*float(-P[1]*T[2]+P[1]*T[4]+P[3]*T[0]-P[3]*T[4]-P[5]*T[0]+P[5]*T[2])                       /div);
    tpCal.f=int32_t(256*float(P[1]*T[2]*T[5]-P[1]*T[3]*T[4]-P[3]*T[0]*T[5]+P[3]*T[1]*T[4]+P[5]*T[0]*T[3]-P[5]*T[1]*T[2])/div);
    saveData((uint8_t*)&tpCal, sizeof(tpCal));
    //int32_t t[6]; memcpy(t, T, 6*4);
    //touchToPoint(t[0], t[1]); touchToPoint(t[2], t[3]); touchToPoint(t[4], t[5]);
    //debug("cal %ld %ld %ld %ld %ld %ld div:%ld", tpCal.a, tpCal.b, tpCal.c, tpCal.d, tpCal.e, tpCal.f, int32_t(div));
    //debug("Ps %d:%d %d:%d %d:%d T:%d:%d %d:%d %d:%d Ret:%d:%d %d:%d %d:%d", int(P[0]), int(P[1]), int(P[2]), int(P[3]), int(P[4]), int(P[5]), int(T[0]), int(T[1]), int(T[2]), int(T[3]), int(T[4]), int(T[5]), int(t[0]), int(t[1]), int(t[2]), int(t[3]), int(t[4]), int(t[5]));
    return true;
}

void CArduViseur::generateTextAndPos()
{
    if (drawFindTextPos!=nullptr) free(drawFindTextPos); drawFindTextPosNb= 0; gotoOnRelease= false;
    if (findMode==1) 
    {
        drawFindTextPos= (TTextAndPos*)malloc((drawFindTextPosNb=110)*sizeof(TTextAndPos));
        for (int i=0; i<110; i++) drawFindTextPos[i].init(messier[i].getString(messierStrings), -1, 'M', ' ', i+1, messier[i].getdec(), radnorm(-messier[i].getra()));
    }
    if (findMode==2) 
    {
        drawFindTextPos= (TTextAndPos*)malloc((drawFindTextPosNb=109)*sizeof(TTextAndPos));
        for (int i=0; i<109; i++) drawFindTextPos[i].init(caldwell[i].getString(cadwellStrings), -1, 'C', ' ', i+1, caldwell[i].getdec(), radnorm(-caldwell[i].getra()));
    }
    if (findMode==3) 
    {
        drawFindTextPosNb= 0;
        TStarMore const *sm= (TStarMore const *)starsMore; 
        for (int i=0; i<nbMoreStars; i++) { if (sm->name!=0) drawFindTextPosNb++; sm= sm->next(); }
        drawFindTextPos= (TTextAndPos*)malloc(drawFindTextPosNb*sizeof(TTextAndPos));
        sm= (TStarMore const *)starsMore; int pos= 0;
        for (int i=0; i<nbMoreStars; i++) 
        { 
            if (sm->name!=0)
                drawFindTextPos[pos++].init(sm->getName(), sm->name, 0, 0, -1, radnorm(stars[sm->index()].dec()-MPI/2.0f), radnorm(-stars[sm->index()].ra()));
            sm= sm->next(); 
        }
    }
    if (findMode==4) 
    {
        drawFindTextPos= (TTextAndPos*)malloc((drawFindTextPosNb=nbMoreStars)*sizeof(TTextAndPos));
        TStarMore const *sm= (TStarMore const *)starsMore; 
        for (int i=0; i<nbMoreStars; i++) 
        { 
            drawFindTextPos[i].init(sm->getName(), sm->name, sm->bayerConst(), sm->bayerGreek(), -2, radnorm(stars[sm->index()].dec()-MPI/2.0f), radnorm(-stars[sm->index()].ra()));
            sm= sm->next(); 
        }
        qsort(drawFindTextPos, drawFindTextPosNb, sizeof(TTextAndPos), TTextAndPos::comp);
    }
}
void CArduViseur::centerRectText(int32_t x, int32_t y, int32_t w, int32_t h, char const *t, int32_t p, TPixel c)
{
    int32_t w2= w/4; x+= w2*p;
    fillRect(x+1, y-1, w2-1, 9, c);
    vLine(x+w2, y-1, h, 0xffff);
    textCenter(x+w2/2, y+4, t, 0xffff);
}
bool CArduViseur::inCenterRectText(int32_t px, int32_t py, int32_t x, int32_t y, int32_t w, int32_t h, int32_t p)
{
    int32_t w2= w/4; x+= w2*p;
    return px>=x && px<x+w2 && py>=y && py<y+10;
}

void CArduViseur::drawFind(int x, int y, int w, int h)
{
    rect(x, y, w, h, 0xffff, 0);
    x++, y+=2, w-= 2, h-= 3;
    // 4 choices M, C, Star names, Star const
    centerRectText(x, y, w, 10, "M", 0, findMode==1 ? 31 : 0);
    centerRectText(x, y, w, 10, "C", 1, findMode==2 ? 31 : 0);
    centerRectText(x, y, w, 10, "Star", 2, findMode==3 ? 31 : 0);
    centerRectText(x, y, w, 10, "Star2", 3, findMode==4 ? 31 : 0);
//    text(x, y, w, " M C Star Star2", 0xffff);
    y+= 8; h-= 10;
    hLine(x, y, w, 0xffff); y+= 2;

    int pos= findMode==1 ? findModeM : (findMode==2 ? findModeC : (findMode==3 ? findModeStarConst : findModeStarConstAlpha));
    int Y= y, X= x+1-pos; int dw= (w-2)/2;
    findModeMax= (drawFindTextPosNb+(h/10)-1)/(h/10)*(dw+2)-w;
    int32_t clip[4]; saveClip(clip, x, y-1, x+w, y+h+2);
    for (int i=0; i<drawFindTextPosNb; i++)
    {
        if (x+dw>=clipx)
        {
            char t[100]= ""; 
            if (drawFindTextPos[i].index>0) sprintf(t, "%c%d%c", drawFindTextPos[i].prefix1, drawFindTextPos[i].index, drawFindTextPos[i].sufix);
            if (drawFindTextPos[i].index==-2) sprintf(t, "%s %s ", bayerGreek[drawFindTextPos[i].sufix], bayerConst[drawFindTextPos[i].prefix1]);
            uint32_t tlen= strlen(t);
            memcpy(t+tlen, drawFindTextPos[i].t, drawFindTextPos[i].tsize); t[tlen+drawFindTextPos[i].tsize]= 0;
            int32_t clip[4]; updateClip(clip, X, Y, X+dw-2, Y+8);
            drawFindTextPos[i].x= int16_t(clipx); drawFindTextPos[i].X= int16_t(clipX); drawFindTextPos[i].y= int16_t(clipy); drawFindTextPos[i].Y= int16_t(clipY); 
            textClip(X, Y, t, 0xffff);
            restoreClip(clip);
        }
        Y+= 10; if (Y+7>=y+h) 
        {
            vLine(X+dw-1, y-1, h+2, 0xffff); 
            Y= y, X+= dw+2;
            if (X+3>=x+w) break;
        }
    }
    restoreClip(clip);
}


//////////////////////////////////////////////
// Planetary stuff
float timeFromDate(int y, int m, int d, int h, int M, int s)
{
    return float(367*y - 7 * ( y + (m+9)/12 ) / 4 + 275*m/9 + d - 730530) + (h+M/60.0f+s/3600.0f)/24.0f;
}

//  N = longitude of the ascending node
//  i = inclination to the ecliptic (plane of the Earth's orbit)
//  w = argument of perihelion
//  a = semi-major axis, or mean distance from Sun
//  e = eccentricity (0=circle, 0-1=ellipse, 1=parabola)
//  M = mean anomaly (0 at perihelion; increases uniformly with time)
struct TOrbitalElements { float N1, N2, i1, i2, w1, w2, a1, a2, e1, e2, M1, M2; };
class TOrbitalElementsAt { public:
    float N, i, w, a, e, M; 
    TOrbitalElementsAt(TOrbitalElements const &o, float d)
    { 
      N= o.N1+o.N2*d; i=o.i1+o.i2*d; w= o.w1+o.w2*d;
      a= o.a1+o.a2*d; e= o.e1+o.e2*d; M= o.M1+o.M1*d;
    }
};
static TOrbitalElements const oe[9]={
    // N                    i                  w                     a                   e                    M
    {0, 0,                  0, 0,              282.9404, 4.70935E-5, 1, 0,               0.016709, -1.151E-9, 356.0470, 0.9856002585}, // sun
    {48.3313, 3.24587E-5,   7.0047, 5.00E-8,   29.1241, 1.01444E-5,  0.387098,0,         0.205635, 5.59E-10,  168.6562, 4.0923344368}, // mercury
    {76.6799, 2.46590E-5,   3.3946, 2.75E-8,   54.8910, 1.38374E-5,  0.723330,0,         0.006773, 1.302E-9,  48.0052, 1.6021302244}, // Venus
    { 49.5574, 2.11081E-5,  1.8497, -1.78E-8,  286.5016, 2.92961E-5, 1.523688,0,         0.093405, 2.516E-9,  18.6021, 0.5240207766}, // Mars
    { 100.4542, 2.76854E-5, 1.3030, -1.557E-7, 273.8777, 1.64505E-5, 5.20256,0,          0.048498, 4.469E-9,  19.8950, 0.0830853001}, // Jupiter
    { 113.6634, 2.38980E-5, 2.4886, -1.081E-7, 339.3939, 2.97661E-5, 9.55475,0,          0.055546, -9.499E-9, 316.9670, 0.0334442282}, // Saturne
    { 74.0005, 1.3978E-5,   0.7733, 1.9E-8,    96.6612, 3.0565E-5,   19.18171, -1.55E-8, 0.047318, 7.45E-9,   142.5905, 0.011725806}, // Uranus
    { 131.7806, 3.0173E-5,  1.7700, -2.55E-7,  272.8461, -6.027E-6,  30.05826, 3.313E-8, 0.008606, 2.15E-9,   260.2471, 0.005995147} // Nepture
};

void sunPos(float d, float &RA, float &Dec)
{
    float ecl= 23.4393 - 3.563E-7 * d; // ecl angle
    TOrbitalElementsAt sun(oe[0], d);
    float E= sun.M+sun.e*(180.0f/MPI)*sinf(sun.M*degToRad)*(1.0+sun.e*cosf(sun.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    // compute the Sun's distance r and its true anomaly v from:
    float xv = cosf(E)-sun.e;
    float yv = sqrt(1.0f-sun.e*sun.e) * sinf(E);
    float v = atan2f( yv, xv );
    float rs = sqrt( xv*xv + yv*yv );
    //compute the Sun's true longitude
    float lonsun = v + sun.w*degToRad;
    // Convert lonsun,r to ecliptic rectangular geocentric coordinates xs,ys:
    float xs = rs * cosf(lonsun);
    float ys = rs * sinf(lonsun);
    // (since the Sun always is in the ecliptic plane, zs is of course zero). xs,ys is the Sun's position in a coordinate system in the plane of the ecliptic. To convert this to equatorial, rectangular, geocentric coordinates, compute:
    float xe = xs;
    float ye = ys * cosf(ecl);
    float ze = ys * sinf(ecl);
    // Finally, compute the Sun's Right Ascension (RA) and Declination (Dec):
    RA  = atan2f( ye, xe );
    Dec = atan2f( ze, sqrt(xe*xe+ye*ye) );
}
void objPos(float d, int ob, float &x, float &y, float &z)
{
    TOrbitalElementsAt o(oe[ob], d);
    float E= (o.M+o.e)*degToRad*sinf(o.M*degToRad)*(1.0+o.e*cosf(o.M*degToRad)); //ccentric anomaly E from the mean anomaly M and from the eccentricity e (E and M in degrees):
    for (int i=0; i<5; i++)
        E= E - ( E - o.e * sinf(E) - o.M*degToRad ) / ( 1 - o.e * cos(E) );
    float xv = o.a * ( cosf(E) - o.e );
    float yv = o.a * ( sqrt(1.0 - o.e*o.e) * sinf(E) );
    float v = atan2f( yv, xv );
    float r = sqrt( xv*xv + yv*yv );
    x = r * ( cosf(o.N*degToRad) * cosf(v+o.w*degToRad) - sinf(o.N*degToRad) * sinf(v+o.w*degToRad) * cosf(o.i*degToRad) );
    y = r * ( sinf(o.N*degToRad) * cosf(v+o.w*degToRad) + cosf(o.N*degToRad) * sinf(v+o.w*degToRad) * cosf(o.i*degToRad) );
    z = r * ( sinf(v+o.w*degToRad) * sin(o.i*degToRad) );
}
//    Related orbital elements are:
//
//  w1 = N + w   = longitude of perihelion
//  L  = M + w1  = mean longitude
//  q  = a*(1-e) = perihelion distance
//  Q  = a*(1+e) = aphelion distance
//  P  = a ^ 1.5 = orbital period (years if a is in AU, astronomical units)
//  T  = Epoch_of_M - (M(deg)/360_deg) / P  = time of perihelion
//  v  = true anomaly (angle between position and perihelion)
//  E  = eccentric anomaly
//  AU=149.6 million km

